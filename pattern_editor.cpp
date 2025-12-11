#include "pattern_editor.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>
#include <regex>

const char* PatternEditor::NOTE_NAMES[] = {
    "C", "C+", "D", "D+", "E", "F",
    "F+", "G", "G+", "A", "A+", "B"
};

const int PatternEditor::NOTE_COUNT = 12;

// Color scheme for notes: 7 base notes with lighter for sharps, darker for flats
// Index: 0=C, 1=C+/D-, 2=D, 3=D+/E-, 4=E, 5=F, 6=F+/G-, 7=G, 8=G+/A-, 9=A, 10=A+/B-, 11=B
// Base notes: C=0, D=2, E=4, F=5, G=7, A=9, B=11
// Sharps: C+=1, D+=3, F+=6, G+=8, A+=10
// Flats: D-=1, E-=3, G-=6, A-=8, B-=10
ImVec4 PatternEditor::GetNoteColor(int note_index, bool is_flat) {
    // Base note colors (RGB values)
    // C=green, D=blue, E=yellow, F=orange, G=purple, A=red, B=cyan
    ImVec4 base_colors[7] = {
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // C - green
        ImVec4(0.2f, 0.4f, 0.9f, 1.0f),  // D - blue
        ImVec4(0.9f, 0.9f, 0.2f, 1.0f),  // E - yellow
        ImVec4(1.0f, 0.6f, 0.2f, 1.0f),  // F - orange
        ImVec4(0.7f, 0.2f, 0.9f, 1.0f),  // G - purple
        ImVec4(0.9f, 0.2f, 0.2f, 1.0f),  // A - red
        ImVec4(0.2f, 0.8f, 0.9f, 1.0f)   // B - cyan
    };
    
    // Map note index to base note (0-6 for C, D, E, F, G, A, B)
    int base_note_map[12] = {0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6}; // C, C+, D, D+, E, F, F+, G, G+, A, A+, B
    int base_index = base_note_map[note_index];
    ImVec4 base_color = base_colors[base_index];
    
    // Determine if it's a sharp or flat
    bool is_sharp = (note_index == 1 || note_index == 3 || note_index == 6 || note_index == 8 || note_index == 10) && !is_flat;
    bool is_flat_note = (note_index == 1 || note_index == 3 || note_index == 6 || note_index == 8 || note_index == 10) && is_flat;
    
    if (is_sharp) {
        // Lighter version for sharps
        return ImVec4(
            std::min(1.0f, base_color.x * 1.3f),
            std::min(1.0f, base_color.y * 1.3f),
            std::min(1.0f, base_color.z * 1.3f),
            1.0f
        );
    } else if (is_flat_note) {
        // Darker version for flats
        return ImVec4(
            base_color.x * 0.6f,
            base_color.y * 0.6f,
            base_color.z * 0.6f,
            1.0f
        );
    } else {
        // Base color for natural notes
        return base_color;
    }
}

PatternEditor::PatternEditor() 
    : m_pattern_length(1)
    , m_note_length(4)
    , m_instrument(-1)
    , m_is_drum_track(false)
    , m_octave(-1)
    , m_selected_pattern_macro(-1)
    , m_has_unsaved_changes(false)
    , m_pattern_name("")
    , m_selected_note(-1)  // Default to rest
    , m_selected_note_is_flat(false)
    , m_selected_octave_change(0)  // Default to no octave change
    , m_open(false)
    , m_request_focus(false)
{
    int total_steps = GetTotalSteps();
    m_pattern.resize(total_steps, -1); // Initialize with rests
    m_is_flat.resize(total_steps, false); // Initialize with sharps (false = sharp)
    m_octave_changes.resize(total_steps, 0); // Initialize with no octave changes
    m_mml_buffer.resize(4096, 0); // Allocate buffer for MML output
    m_pattern_name_buffer.resize(256, 0); // Allocate buffer for pattern name input
    UpdateMML();
}

int PatternEditor::GetStepsPerBar() const {
    // In MML, the note length value represents how many of that note fit in a whole note
    // In a 4/4 bar (4 beats), the number of steps per bar equals the note length value:
    // - l1 (whole note): 1 step per bar
    // - l2 (half note): 2 steps per bar
    // - l4 (quarter note): 4 steps per bar
    // - l8 (eighth note): 8 steps per bar
    // - l16 (sixteenth note): 16 steps per bar
    // - l32 (thirty-second note): 32 steps per bar
    return m_note_length;
}

int PatternEditor::GetTotalSteps() const {
    return m_pattern_length * GetStepsPerBar();
}

PatternEditor::~PatternEditor() {
}

void PatternEditor::SetEditorText(const std::string& text) {
    // Only reset if the text actually changed (not just a refresh with same content)
    if (m_editor_text != text) {
        // If we have unsaved changes, don't reset (user is editing)
        if (!m_has_unsaved_changes) {
            m_editor_text = text; 
            m_modified_editor_text = text;
            m_selected_pattern_macro = -1;
        } else {
            // Text changed externally while editing - this shouldn't happen normally
            // but if it does, we'll update the base text but keep the selection
            m_editor_text = text;
        }
    }
}

std::vector<PatternEditor::PatternInfo> PatternEditor::ScanForPatterns(const std::string& text) {
    std::vector<PatternInfo> patterns;
    
    // Find all pattern macros (*701 through *799)
    // Pattern format: *701 @1 o3 l4 a b c d | e f g a; (semicolon optional)
    // We'll find each *number and extract content until next *number or end
    // IMPORTANT: Only match macros that start at the beginning of a line (after optional whitespace)
    std::regex pattern_macro_regex(R"(\*(\d+))");
    std::sregex_iterator iter(text.begin(), text.end(), pattern_macro_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
        std::smatch match = *iter;
        std::string macro_number_str = match[1].str();
        
        // Only process macros from 701 to 799
        int macro_number = 0;
        try {
            macro_number = std::stoi(macro_number_str);
        } catch (...) {
            continue; // Skip invalid macro numbers
        }
        
        if (macro_number < 701 || macro_number > 799) {
            continue; // Skip macros outside the 701-799 range
        }
        
        // Find the start of the line containing this macro
        size_t macro_pos = match.position();
        size_t line_start = macro_pos;
        // Go back to find the start of the line
        while (line_start > 0 && text[line_start - 1] != '\n' && text[line_start - 1] != '\r') {
            line_start--;
        }
        
        // Check if the macro is at the start of the line (allowing only whitespace before it)
        // Skip if there are non-whitespace characters before the macro on the same line
        bool is_at_line_start = true;
        for (size_t i = line_start; i < macro_pos; ++i) {
            if (!std::isspace(text[i])) {
                is_at_line_start = false;
                break;
            }
        }
        
        if (!is_at_line_start) {
            continue; // Skip macros that are not at the start of the line
        }
        
        // Find the content after this macro number - patterns must be on a single line
        size_t macro_end_pos = macro_pos + match.length();
        std::string pattern_content;
        
        // Find the end of the line containing this macro
        size_t line_end = macro_end_pos;
        // Find the end of the line
        while (line_end < text.length() && text[line_end] != '\n' && text[line_end] != '\r') {
            line_end++;
        }
        
        // Extract content from after macro number to end of line
        size_t content_start = macro_end_pos;
        while (content_start < line_end && std::isspace(text[content_start])) {
            content_start++;
        }
        
        // Extract the content (everything on this line after the macro number)
        if (line_end > content_start) {
            pattern_content = text.substr(content_start, line_end - content_start);
        }
        
        // Extract the name after semicolon (if present) - must be on the same line
        std::string pattern_name;
        size_t semicolon_pos = pattern_content.find(';');
        if (semicolon_pos != std::string::npos) {
            size_t name_start = semicolon_pos + 1;
            // Skip whitespace after semicolon
            while (name_start < pattern_content.length() && std::isspace(pattern_content[name_start])) {
                name_start++;
            }
            // Name is everything after semicolon until end of line
            if (name_start < pattern_content.length()) {
                pattern_name = pattern_content.substr(name_start);
                // Trim whitespace from name
                while (!pattern_name.empty() && std::isspace(pattern_name[0])) {
                    pattern_name.erase(0, 1);
                }
                while (!pattern_name.empty() && std::isspace(pattern_name[pattern_name.length() - 1])) {
                    pattern_name.erase(pattern_name.length() - 1, 1);
                }
            }
            // Remove name from pattern_content (keep only up to semicolon)
            pattern_content = pattern_content.substr(0, semicolon_pos);
        }
        
        // Remove [|] and [] markers from pattern content
        std::string cleaned_content = pattern_content;
        // Remove [|] marker
        size_t pos = cleaned_content.find("[|]");
        while (pos != std::string::npos) {
            cleaned_content.erase(pos, 3);
            pos = cleaned_content.find("[|]");
        }
        // Remove [] marker
        pos = cleaned_content.find("[]");
        while (pos != std::string::npos) {
            cleaned_content.erase(pos, 2);
            pos = cleaned_content.find("[]");
        }
        
        // Trim whitespace
        while (!cleaned_content.empty() && std::isspace(cleaned_content[0])) {
            cleaned_content.erase(0, 1);
        }
        while (!cleaned_content.empty() && std::isspace(cleaned_content[cleaned_content.length() - 1])) {
            cleaned_content.erase(cleaned_content.length() - 1, 1);
        }
        
        // Skip empty patterns
        if (cleaned_content.empty()) {
            continue;
        }
        
        PatternInfo info;
        info.content = cleaned_content;
        info.instrument = -1;
        info.is_drum_track = false;
        info.octave = -1;
        info.note_length = 4; // default
        info.bars = 1; // default
        info.macro_number = macro_number; // Store the macro number
        info.name = pattern_name; // Store the pattern name
        
        // Parse the pattern content
        std::istringstream iss(cleaned_content);
        std::string token;
        
        while (iss >> token) {
            // Check for instrument @X
            if (token[0] == '@' && token.length() > 1) {
                try {
                    info.instrument = std::stoi(token.substr(1));
                    info.is_drum_track = false;
                } catch (...) {
                    info.instrument = -1;
                }
            }
            // Check for drum track DX
            else if (token[0] == 'D' && token.length() > 1) {
                try {
                    info.instrument = std::stoi(token.substr(1));
                    info.is_drum_track = true;
                } catch (...) {
                    info.instrument = -1;
                }
            }
            // Check for octave oX
            else if (token[0] == 'o' && token.length() > 1) {
                try {
                    info.octave = std::stoi(token.substr(1));
                } catch (...) {
                    info.octave = -1;
                }
            }
            // Check for note length lX
            else if (token[0] == 'l' && token.length() > 1) {
                try {
                    info.note_length = std::stoi(token.substr(1));
                } catch (...) {
                    info.note_length = 4;
                }
            }
        }
        
        // Count bars by counting | separators
        // Each | separator indicates a new bar, so bars = separator count + 1
        size_t separator_count = std::count(cleaned_content.begin(), cleaned_content.end(), '|');
        info.bars = separator_count > 0 ? separator_count + 1 : 1;
        
        patterns.push_back(info);
    }
    
    // Sort patterns by macro number to ensure correct order
    std::sort(patterns.begin(), patterns.end(), 
        [](const PatternInfo& a, const PatternInfo& b) {
            return a.macro_number < b.macro_number;
        });
    
    return patterns;
}

bool PatternEditor::LoadPattern(const PatternInfo& pattern) {
    // Track which pattern we're editing
    m_selected_pattern_macro = pattern.macro_number;
    // Keep name as-is (empty if no name in MML)
    m_pattern_name = pattern.name;
    m_has_unsaved_changes = false;
    m_modified_editor_text = m_editor_text;  // Start with original text
    
    // Set pattern length (bars)
    m_pattern_length = pattern.bars;
    
    // Set note length
    m_note_length = pattern.note_length;
    
    // Set instrument
    m_instrument = pattern.instrument;
    m_is_drum_track = pattern.is_drum_track;
    
    // Set octave
    m_octave = pattern.octave;
    
    // Resize pattern arrays
    int total_steps = GetTotalSteps();
    m_pattern.resize(total_steps, -1);
    m_is_flat.resize(total_steps, false);
    m_octave_changes.resize(total_steps, 0);
    
    // Parse the pattern content character by character to handle concatenated notes
    // Extract the note sequence part by skipping @, o, l commands and | separators
    std::string note_sequence;
    std::string content = pattern.content;
    size_t content_pos = 0;
    
    // Skip commands (@X, oX, lX) and separators (|)
    while (content_pos < content.length()) {
        // Skip whitespace
        if (std::isspace(content[content_pos])) {
            content_pos++;
            continue;
        }
        
        // Skip @X, DX, oX, lX commands
        if (content[content_pos] == '@' || content[content_pos] == 'D' || content[content_pos] == 'o' || content[content_pos] == 'l') {
            content_pos++;
            // Skip digits after command
            while (content_pos < content.length() && std::isdigit(content[content_pos])) {
                content_pos++;
            }
            continue;
        }
        
        // Skip | separator
        if (content[content_pos] == '|') {
            content_pos++;
            continue;
        }
        
        // Everything else is part of the note sequence
        note_sequence += content[content_pos];
        content_pos++;
    }
    
    // Parse notes character by character
    int step_index = 0;
    size_t pos = 0;
    while (pos < note_sequence.length() && step_index < total_steps) {
        // Skip whitespace
        if (std::isspace(note_sequence[pos])) {
            pos++;
            continue;
        }
        
        // Handle octave changes
        if (note_sequence[pos] == '<') {
            m_octave_changes[step_index] = -1;
            pos++;
            continue;
        } else if (note_sequence[pos] == '>') {
            m_octave_changes[step_index] = 1;
            pos++;
            continue;
        }
        
        // Handle rest
        if (std::tolower(note_sequence[pos]) == 'r') {
            m_pattern[step_index] = -1;
            pos++;
            step_index++;
            continue;
        }
        
        // Handle tie
        if (note_sequence[pos] == '^') {
            m_pattern[step_index] = -2;
            pos++;
            step_index++;
            continue;
        }
        
        // Parse note (a-g, optionally followed by + or -)
        char note_char = std::tolower(note_sequence[pos]);
        if (note_char >= 'a' && note_char <= 'g') {
            pos++;
            bool is_sharp = false;
            bool is_flat = false;
            
            // Check for sharp or flat
            if (pos < note_sequence.length()) {
                if (note_sequence[pos] == '+') {
                    is_sharp = true;
                    pos++;
                } else if (note_sequence[pos] == '-') {
                    is_flat = true;
                    pos++;
                }
            }
            
            // Map note to index
            int note_index = -1;
            if (note_char == 'c') {
                note_index = is_sharp ? 1 : 0;
                if (is_sharp) m_is_flat[step_index] = false;
            } else if (note_char == 'd') {
                note_index = is_flat ? 1 : (is_sharp ? 3 : 2);
                m_is_flat[step_index] = is_flat;
            } else if (note_char == 'e') {
                note_index = is_flat ? 3 : 4;
                m_is_flat[step_index] = is_flat;
            } else if (note_char == 'f') {
                note_index = is_sharp ? 6 : 5;
                if (is_sharp) m_is_flat[step_index] = false;
            } else if (note_char == 'g') {
                note_index = is_flat ? 6 : (is_sharp ? 8 : 7);
                m_is_flat[step_index] = is_flat;
            } else if (note_char == 'a') {
                note_index = is_flat ? 8 : (is_sharp ? 10 : 9);
                m_is_flat[step_index] = is_flat;
            } else if (note_char == 'b') {
                note_index = is_flat ? 10 : 11;
                m_is_flat[step_index] = is_flat;
            }
            
            if (note_index >= 0) {
                m_pattern[step_index] = note_index;
            }
            step_index++;
        } else {
            // Unknown character, skip it
            pos++;
        }
    }
    
    UpdateMML();
    return true;
}

void PatternEditor::UpdateMML() {
    std::ostringstream oss;
    
    // Output instrument or drum track number if specified (must be >= 1)
    if (m_instrument >= 1) {
        if (m_is_drum_track) {
            oss << "D" << m_instrument << " ";
        } else {
            oss << "@" << m_instrument << " ";
        }
    }
    
    // Output starting octave if specified (must be 2-9)
    if (m_octave >= 2 && m_octave <= 9) {
        oss << "o" << m_octave << " ";
    }
    
    // Output the default note length command
    oss << "l" << m_note_length;
    
    // Output all notes/rests without length suffixes
    int total_steps = GetTotalSteps();
    int steps_per_bar = GetStepsPerBar();
    
    for (int i = 0; i < total_steps; ++i) {
        oss << " ";
        
        // Output octave change if any
        int octave_change = m_octave_changes[i];
        if (octave_change == -1) {
            oss << "<";
        } else if (octave_change == 1) {
            oss << ">";
        }
        
        int note = m_pattern[i];
        if (note == -2) {
            // Output a tie
            oss << "^";
        } else if (note >= 0 && note < NOTE_COUNT) {
            // Output just the note name (lowercase)
            // Sharps use +, flats use - in MML
            const char* note_base_names[] = {
                "c", "d", "e", "f", "g", "a", "b"
            };
            
            // Map note indices to base notes and accidentals
            // 0=C, 1=C#/Db, 2=D, 3=D#/Eb, 4=E, 5=F, 6=F#/Gb, 7=G, 8=G#/Ab, 9=A, 10=A#/Bb, 11=B
            if (note == 0) oss << "c";
            else if (note == 1) {
                if (m_is_flat[i]) oss << "d-";  // Db
                else oss << "c+";  // C#
            }
            else if (note == 2) oss << "d";
            else if (note == 3) {
                if (m_is_flat[i]) oss << "e-";  // Eb
                else oss << "d+";  // D#
            }
            else if (note == 4) oss << "e";
            else if (note == 5) oss << "f";
            else if (note == 6) {
                if (m_is_flat[i]) oss << "g-";  // Gb
                else oss << "f+";  // F#
            }
            else if (note == 7) oss << "g";
            else if (note == 8) {
                if (m_is_flat[i]) oss << "a-";  // Ab
                else oss << "g+";  // G#
            }
            else if (note == 9) oss << "a";
            else if (note == 10) {
                if (m_is_flat[i]) oss << "b-";  // Bb
                else oss << "a+";  // A#
            }
            else if (note == 11) oss << "b";
        } else {
            // Output a rest
            oss << "r";
        }
        
        // Add bar separator after each complete bar (except the last one)
        if (m_pattern_length > 1 && (i + 1) % steps_per_bar == 0 && (i + 1) < total_steps) {
            oss << " |";
        }
    }
    
    m_mml_output = oss.str();
    
    // Update buffer for ImGui
    size_t required_size = m_mml_output.size() + 1;
    if (m_mml_buffer.size() < required_size) {
        m_mml_buffer.resize(std::max(required_size, size_t(4096)));
    }
    std::copy(m_mml_output.begin(), m_mml_output.end(), m_mml_buffer.begin());
    m_mml_buffer[m_mml_output.size()] = '\0';
    
    // Mark as having unsaved changes if a pattern is selected
    if (m_selected_pattern_macro >= 701) {
        m_has_unsaved_changes = true;
    }
}

void PatternEditor::ApplyPatternChanges() {
    if (m_selected_pattern_macro < 701) {
        return; // No pattern selected
    }
    
    // Find the pattern macro in the editor text
    std::string macro_str = "*" + std::to_string(m_selected_pattern_macro);
    size_t macro_pos = m_editor_text.find(macro_str);
    
    // If pattern doesn't exist, insert it after the previous pattern
    if (macro_pos == std::string::npos) {
        // Find where to insert the new pattern
        size_t insert_pos = 0;
        
        // Find the previous pattern (the one with the highest macro number less than current)
        std::vector<PatternInfo> found_patterns = ScanForPatterns(m_editor_text);
        int prev_macro = -1;
        size_t prev_pattern_end = std::string::npos;
        
        for (const auto& pat : found_patterns) {
            if (pat.macro_number < m_selected_pattern_macro && pat.macro_number > prev_macro) {
                prev_macro = pat.macro_number;
                std::string prev_macro_str = "*" + std::to_string(pat.macro_number);
                size_t prev_macro_pos = m_editor_text.find(prev_macro_str);
                if (prev_macro_pos != std::string::npos) {
                    // Find the end of this pattern's line (after name)
                    prev_pattern_end = prev_macro_pos;
                    while (prev_pattern_end < m_editor_text.length() && 
                           m_editor_text[prev_pattern_end] != '\n' && 
                           m_editor_text[prev_pattern_end] != '\r') {
                        prev_pattern_end++;
                    }
                    // Include the newline
                    if (prev_pattern_end < m_editor_text.length()) {
                        if (m_editor_text[prev_pattern_end] == '\r') {
                            prev_pattern_end++;
                            if (prev_pattern_end < m_editor_text.length() && m_editor_text[prev_pattern_end] == '\n') {
                                prev_pattern_end++;
                            }
                        } else if (m_editor_text[prev_pattern_end] == '\n') {
                            prev_pattern_end++;
                        }
                    }
                }
            }
        }
        
        if (prev_pattern_end != std::string::npos) {
            insert_pos = prev_pattern_end;
        } else {
            // No previous pattern, insert at the beginning
            insert_pos = 0;
        }
        
        // Build the new pattern
        int pattern_number = m_selected_pattern_macro - 700;
        std::string new_pattern = macro_str + " " + m_mml_output + ";";
        std::string name_to_add = m_pattern_name.empty() ? std::to_string(pattern_number) : m_pattern_name;
        new_pattern += " " + name_to_add + "\n";
        
        // Insert the pattern
        m_modified_editor_text = m_editor_text.substr(0, insert_pos) + 
                                new_pattern + 
                                m_editor_text.substr(insert_pos);
        m_editor_text = m_modified_editor_text;
        m_has_unsaved_changes = false;
        return;
    }
    
    // Find the start of the pattern content (after macro number)
    size_t content_start = macro_pos + macro_str.length();
    while (content_start < m_editor_text.length() && std::isspace(m_editor_text[content_start])) {
        content_start++;
    }
    
    // Find the end of the pattern (semicolon, next *number, or end of line)
    size_t content_end = content_start;
    bool found_semicolon = false;
    size_t line_end = content_start;
    
    // First, find the end of the current line
    while (line_end < m_editor_text.length() && m_editor_text[line_end] != '\n' && m_editor_text[line_end] != '\r') {
        line_end++;
    }
    
    // Now search for semicolon or next macro within the line
    while (content_end < line_end) {
        if (m_editor_text[content_end] == ';') {
            found_semicolon = true;
            content_end++; // Include semicolon in replacement
            break;
        }
        content_end++;
    }
    
    // If no semicolon found, check if there's a next macro on the next line
    if (!found_semicolon) {
        // Skip to start of next line
        size_t next_line_start = line_end;
        if (next_line_start < m_editor_text.length() && m_editor_text[next_line_start] == '\r') {
            next_line_start++;
        }
        if (next_line_start < m_editor_text.length() && m_editor_text[next_line_start] == '\n') {
            next_line_start++;
        }
        // Skip whitespace
        while (next_line_start < m_editor_text.length() && std::isspace(m_editor_text[next_line_start])) {
            next_line_start++;
        }
        // Check if it's a macro
        if (next_line_start < m_editor_text.length() && m_editor_text[next_line_start] == '*') {
            // Check if it's followed by digits (701-799)
            size_t check_pos = next_line_start + 1;
            while (check_pos < m_editor_text.length() && std::isdigit(m_editor_text[check_pos])) {
                check_pos++;
            }
            if (check_pos > next_line_start + 1) {
                // Found a new macro, stop at end of current line
                content_end = line_end;
            } else {
                // Not a macro, stop at end of current line anyway
                content_end = line_end;
            }
        } else {
            // No next macro, stop at end of current line
            content_end = line_end;
        }
    }
    
    // Build the new pattern content - always end with semicolon
    std::string new_pattern_content = m_mml_output + ";";
    
    // Find where the name starts (after semicolon) to preserve it - must be on same line
    size_t name_start = found_semicolon ? content_end : std::string::npos;
    size_t name_end = name_start;
    
    if (found_semicolon && name_start < line_end) {
        // Skip whitespace after semicolon
        while (name_start < line_end && std::isspace(m_editor_text[name_start])) {
            name_start++;
        }
        // Name ends at end of line (don't go beyond line_end)
        name_end = line_end;
    }
    
    // Add the pattern name (or pattern number if blank)
    int pattern_number = m_selected_pattern_macro - 700;
    std::string name_to_add;
    if (!m_pattern_name.empty()) {
        name_to_add = m_pattern_name;
    } else if (found_semicolon && name_start < name_end) {
        // Preserve existing name if we're not changing it
        name_to_add = m_editor_text.substr(name_start, name_end - name_start);
        // Trim whitespace
        while (!name_to_add.empty() && std::isspace(name_to_add[0])) {
            name_to_add.erase(0, 1);
        }
        while (!name_to_add.empty() && std::isspace(name_to_add[name_to_add.length() - 1])) {
            name_to_add.erase(name_to_add.length() - 1, 1);
        }
        // If still empty after trimming, use pattern number
        if (name_to_add.empty()) {
            name_to_add = std::to_string(pattern_number);
        }
    } else {
        // No existing name, use pattern number
        name_to_add = std::to_string(pattern_number);
    }
    
    if (!name_to_add.empty()) {
        new_pattern_content += " " + name_to_add;
    }
    
    // Determine where to stop replacing - line_end points to newline or end of string
    size_t replace_end = line_end;
    // Check if there's a newline to preserve
    bool has_newline = (replace_end < m_editor_text.length() && 
                       (m_editor_text[replace_end] == '\n' || m_editor_text[replace_end] == '\r'));
    
    // Add newline to the new pattern content
    if (has_newline) {
        // Preserve the original newline style
        if (replace_end < m_editor_text.length() && m_editor_text[replace_end] == '\r') {
            new_pattern_content += "\r";
            replace_end++;
            if (replace_end < m_editor_text.length() && m_editor_text[replace_end] == '\n') {
                new_pattern_content += "\n";
                replace_end++;
            }
        } else if (replace_end < m_editor_text.length() && m_editor_text[replace_end] == '\n') {
            new_pattern_content += "\n";
            replace_end++;
        }
    } else {
        // No newline at end of file, add one
        new_pattern_content += "\n";
    }
    
    // Replace the pattern content in the editor text (only replace the line, preserve everything after)
    m_modified_editor_text = m_editor_text.substr(0, content_start) + 
                            new_pattern_content + 
                            m_editor_text.substr(replace_end);
    
    // Update the original editor text
    m_editor_text = m_modified_editor_text;
    m_has_unsaved_changes = false;
}

void PatternEditor::CancelPatternChanges() {
    if (m_selected_pattern_macro < 701) {
        return; // No pattern selected
    }
    
    // Reload the original pattern
    std::vector<PatternInfo> found_patterns = ScanForPatterns(m_editor_text);
    for (const auto& pat : found_patterns) {
        if (pat.macro_number == m_selected_pattern_macro) {
            LoadPattern(pat);
            break;
        }
    }
    
    m_modified_editor_text = m_editor_text;
    m_has_unsaved_changes = false;
}

void PatternEditor::CreateDefaultPattern() {
    // Check if *701 already exists
    if (m_editor_text.find("*701") != std::string::npos) {
        return; // Already exists, don't create
    }
    
    // Create a default *701 pattern with a single rest and pattern name "1"
    std::string default_pattern = "*701 l1 r ; 1\n";
    
    // Insert at the beginning of the editor text
    if (m_editor_text.empty()) {
        m_editor_text = default_pattern;
    } else {
        // Insert at the top
        m_editor_text = default_pattern + m_editor_text;
    }
    
    // Update modified text as well
    m_modified_editor_text = m_editor_text;
}

int PatternEditor::FindNextAvailableMacro() {
    std::vector<PatternInfo> found_patterns = ScanForPatterns(m_editor_text);
    std::vector<int> used_macros;
    for (const auto& pat : found_patterns) {
        used_macros.push_back(pat.macro_number);
    }
    
    // Find the first available macro from 701 to 799
    for (int i = 701; i <= 799; ++i) {
        bool found = false;
        for (int used : used_macros) {
            if (used == i) {
                found = true;
                break;
            }
        }
        if (!found) {
            return i;
        }
    }
    
    return -1; // No available macros
}

void PatternEditor::CreateNewPattern(bool copy_current) {
    int new_macro = FindNextAvailableMacro();
    if (new_macro == -1) {
        return; // No available macros
    }
    
    std::string new_pattern;
    if (copy_current && m_selected_pattern_macro >= 701) {
        // Copy current pattern
        new_pattern = "*" + std::to_string(new_macro) + " " + m_mml_output + ";";
        if (!m_pattern_name.empty()) {
            new_pattern += " " + m_pattern_name;
        }
    } else {
        // Create clean pattern
        new_pattern = "*" + std::to_string(new_macro) + " l1 r;";
    }
    new_pattern += "\n";
    
    // Insert at the beginning of the editor text
    if (m_editor_text.empty()) {
        m_editor_text = new_pattern;
    } else {
        m_editor_text = new_pattern + m_editor_text;
    }
    
    m_modified_editor_text = m_editor_text;
    
    // Load the new pattern
    std::vector<PatternInfo> found_patterns = ScanForPatterns(m_editor_text);
    for (const auto& pat : found_patterns) {
        if (pat.macro_number == new_macro) {
            LoadPattern(pat);
            break;
        }
    }
}

std::string PatternEditor::NoteToMML(int note_index, int note_length) {
    if (note_index < 0 || note_index >= NOTE_COUNT) {
        return "";
    }
    
    // Convert note index to MML note name (lowercase)
    const char* note_chars[] = {
        "c", "c#", "d", "d#", "e", "f",
        "f#", "g", "g#", "a", "a#", "b"
    };
    
    std::string result = note_chars[note_index];
    result += std::to_string(note_length);
    
    return result;
}

void PatternEditor::Render() {
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
    
    if (m_request_focus) {
        ImGui::SetNextWindowFocus();
        m_request_focus = false;
    }
    
    if (ImGui::Begin("Pattern Editor", &m_open)) {
        // Pattern list from editor (shown at top)
        std::vector<PatternInfo> found_patterns = ScanForPatterns(m_editor_text);
        
        // If no patterns exist, create a default *701 pattern
        if (found_patterns.empty()) {
            CreateDefaultPattern();
            // Re-scan to get the newly created pattern
            found_patterns = ScanForPatterns(m_editor_text);
            // Automatically load the default pattern
            if (!found_patterns.empty()) {
                LoadPattern(found_patterns[0]);
            }
        }
        
        if (!found_patterns.empty()) {
            ImGui::Text("Found Patterns:");
            if (ImGui::BeginListBox("##PatternList", ImVec2(-1, 100))) {
                for (size_t i = 0; i < found_patterns.size(); ++i) {
                    const auto& pat = found_patterns[i];
                    // Pattern number is macro_number - 700 (701 = pattern 1, 702 = pattern 2, etc.)
                    int pattern_number = pat.macro_number - 700;
                    std::string label = "Pattern " + std::to_string(pattern_number) + " (*" + std::to_string(pat.macro_number) + ")";
                    // Show name, or pattern number if name is blank
                    std::string display_name = pat.name.empty() ? std::to_string(pattern_number) : pat.name;
                    label += " - " + display_name;
                    if (pat.instrument >= 1) {
                        if (pat.is_drum_track) {
                            label += " D" + std::to_string(pat.instrument);
                        } else {
                            label += " @" + std::to_string(pat.instrument);
                        }
                    }
                    if (pat.octave >= 2 && pat.octave <= 9) {
                        label += " o" + std::to_string(pat.octave);
                    }
                    label += " l" + std::to_string(pat.note_length);
                    label += " (" + std::to_string(pat.bars) + " bars)";
                    
                    if (ImGui::Selectable(label.c_str())) {
                        LoadPattern(pat);
                    }
                }
                ImGui::EndListBox();
            }
            
            // Buttons to create new patterns
            if (ImGui::Button("New Pattern (Clean)")) {
                CreateNewPattern(false);
            }
            ImGui::SameLine();
            if (ImGui::Button("New Pattern (Copy Current)")) {
                CreateNewPattern(true);
            }
            
            ImGui::Separator();
        }
        
        // Show currently selected pattern and Apply/Cancel buttons
        if (m_selected_pattern_macro >= 701) {
            int pattern_number = m_selected_pattern_macro - 700;
            ImGui::Text("Editing Pattern %d (*%d)", pattern_number, m_selected_pattern_macro);
            if (m_has_unsaved_changes) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(Unsaved changes)");
            }
            
            ImGui::SameLine(ImGui::GetWindowWidth() - 200);
            if (ImGui::Button("Apply", ImVec2(80, 0))) {
                ApplyPatternChanges();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                CancelPatternChanges();
            }
            
            // Pattern name input
            ImGui::Text("Pattern Name (optional):");
            ImGui::SameLine(0, 10.0f); // Add 10 pixels spacing after label
            static int last_selected_macro = -1;
            if (m_selected_pattern_macro != last_selected_macro) {
                // Update buffer when pattern changes - show pattern number if name is empty
                std::fill(m_pattern_name_buffer.begin(), m_pattern_name_buffer.end(), 0);
                std::string name_to_show = m_pattern_name.empty() ? std::to_string(pattern_number) : m_pattern_name;
                std::copy(name_to_show.begin(), name_to_show.end(), m_pattern_name_buffer.begin());
                last_selected_macro = m_selected_pattern_macro;
            }
            ImGui::SetNextItemWidth(250); // Increased from 200
            if (ImGui::InputText("##PatternName", m_pattern_name_buffer.data(), m_pattern_name_buffer.size())) {
                std::string input_name = std::string(m_pattern_name_buffer.data());
                // If user set it to the pattern number or cleared it, treat as empty (will auto-use pattern number)
                if (input_name.empty() || input_name == std::to_string(pattern_number)) {
                    m_pattern_name = "";
                } else {
                    m_pattern_name = input_name;
                }
                m_has_unsaved_changes = true;
            }
            
            ImGui::Separator();
        }
        
        // Calculate total steps once at the start
        int total_steps = GetTotalSteps();
        
        // Pattern length selector (in bars)
        ImGui::Text("Pattern Length (bars):");
        ImGui::SameLine(0, 10.0f); // Add 10 pixels spacing after label
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputInt("##PatternLength", &m_pattern_length, 1, 1)) {
            m_pattern_length = std::max(1, std::min(16, m_pattern_length));
            total_steps = GetTotalSteps();
            m_pattern.resize(total_steps, -1);
            m_is_flat.resize(total_steps, false);
            m_octave_changes.resize(total_steps, 0);
            UpdateMML();
        }
        
        ImGui::Spacing(); // Add vertical spacing between rows
        
        // Note length selector
        ImGui::Text("Note Length:");
        ImGui::SameLine(0, 10.0f); // Add 10 pixels spacing after label
        const char* note_length_names[] = { "1 (Whole)", "2 (Half)", "4 (Quarter)", "8 (Eighth)", "16 (Sixteenth)", "32 (Thirty-second)" };
        int note_length_values[] = { 1, 2, 4, 8, 16, 32 };
        int current_index = 0;
        for (int i = 0; i < 6; ++i) {
            if (note_length_values[i] == m_note_length) {
                current_index = i;
                break;
            }
        }
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("##NoteLength", &current_index, note_length_names, 6)) {
            m_note_length = note_length_values[current_index];
            // Resize pattern arrays when note length changes
            total_steps = GetTotalSteps();
            m_pattern.resize(total_steps, -1);
            m_is_flat.resize(total_steps, false);
            m_octave_changes.resize(total_steps, 0);
            UpdateMML();
        }
        
        ImGui::Spacing(); // Add vertical spacing between rows
        
        // Instrument/Drum track selector
        ImGui::Text("Instrument:");
        ImGui::SameLine(0, 10.0f); // Add 10 pixels spacing after label
        bool has_instrument = (m_instrument >= 1);
        if (ImGui::Checkbox("##InstrumentEnabled", &has_instrument)) {
            if (has_instrument && m_instrument < 1) {
                m_instrument = 1; // Default to instrument 1 if enabling
            } else if (!has_instrument) {
                m_instrument = -1; // Disable instrument
            }
            UpdateMML();
        }
        if (has_instrument) {
            ImGui::SameLine(0, 8.0f); // Add 8 pixels spacing after checkbox
            // Choose between @ (instrument) and D (drum track)
            const char* instrument_types[] = { "@", "D" };
            int type_index = m_is_drum_track ? 1 : 0;
            ImGui::SetNextItemWidth(60); // Increased from 40
            if (ImGui::Combo("##InstrumentType", &type_index, instrument_types, 2)) {
                m_is_drum_track = (type_index == 1);
                UpdateMML();
            }
            ImGui::SameLine(0, 8.0f); // Add 8 pixels spacing after type selector
            ImGui::SetNextItemWidth(100); // Increased from 80
            int instrument_val = m_instrument;
            if (ImGui::InputInt("##InstrumentNumber", &instrument_val, 1, 1)) {
                m_instrument = std::max(1, instrument_val);
                UpdateMML();
            }
        } else {
            ImGui::SameLine(0, 8.0f); // Add 8 pixels spacing after checkbox
            ImGui::TextDisabled("(none)");
        }
        
        ImGui::Spacing(); // Add vertical spacing between rows
        
        // Octave selector
        ImGui::Text("Octave:");
        ImGui::SameLine(0, 10.0f); // Add 10 pixels spacing after label
        bool has_octave = (m_octave >= 2 && m_octave <= 9);
        if (ImGui::Checkbox("##OctaveEnabled", &has_octave)) {
            if (has_octave && (m_octave < 2 || m_octave > 9)) {
                m_octave = 3; // Default to octave 3 if enabling
            } else if (!has_octave) {
                m_octave = -1; // Disable octave
            }
            UpdateMML();
        }
        if (has_octave) {
            ImGui::SameLine(0, 8.0f); // Add 8 pixels spacing after checkbox
            ImGui::SetNextItemWidth(100); // Increased from 80
            int octave_val = m_octave;
            if (ImGui::InputInt("##OctaveNumber", &octave_val, 1, 1)) {
                m_octave = std::max(2, std::min(9, octave_val));
                UpdateMML();
            }
        } else {
            ImGui::SameLine(0, 8.0f); // Add 8 pixels spacing after checkbox
            ImGui::TextDisabled("(none)");
        }
        
        ImGui::Separator();
        
        // Note selector with colored buttons
        ImGui::Text("Select Note/Option:");
        ImGui::Spacing();
        
        // Rest and Tie options (gray buttons)
        bool is_rest_selected = (m_selected_note == -1 && m_selected_octave_change == 0);
        bool is_tie_selected = (m_selected_note == -2 && m_selected_octave_change == 0);
        
        if (is_rest_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        }
        if (ImGui::Button("Rest (R)", ImVec2(80, 30))) {
            m_selected_note = -1;
            m_selected_note_is_flat = false;
            m_selected_octave_change = 0;
        }
        if (is_rest_selected) {
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 10.0f);
        
        if (is_tie_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
        }
        if (ImGui::Button("Tie (^)", ImVec2(80, 30))) {
            m_selected_note = -2;
            m_selected_note_is_flat = false;
            m_selected_octave_change = 0;
        }
        if (is_tie_selected) {
            ImGui::PopStyleColor(3);
        }
        
        ImGui::Spacing();
        ImGui::Text("Notes:");
        
        // Helper function to render a note button
        auto render_note_button = [&](int note_index, bool is_flat, const std::string& label, float width = 45.0f) {
            bool is_selected = (m_selected_note == note_index && m_selected_note_is_flat == is_flat && m_selected_octave_change == 0);
            ImVec4 note_color = GetNoteColor(note_index, is_flat);
            ImVec4 hover_color = ImVec4(
                std::min(1.0f, note_color.x * 1.2f),
                std::min(1.0f, note_color.y * 1.2f),
                std::min(1.0f, note_color.z * 1.2f),
                1.0f
            );
            ImVec4 active_color = ImVec4(
                note_color.x * 0.8f,
                note_color.y * 0.8f,
                note_color.z * 0.8f,
                1.0f
            );
            
            ImGui::PushStyleColor(ImGuiCol_Button, note_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);
            
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
            }
            
            if (ImGui::Button(label.c_str(), ImVec2(width, 30))) {
                m_selected_note = note_index;
                m_selected_note_is_flat = is_flat;
                m_selected_octave_change = 0;
            }
            
            if (is_selected) {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
            }
            
            ImGui::PopStyleColor(3);
        };
        
        // Display notes grouped with their sharps and flats across two rows
        // Row 1
        render_note_button(0, false, "C", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(1, false, "C+", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(1, true, "D-", 45.0f);
        ImGui::SameLine(0, 8.0f);
        
        render_note_button(2, false, "D", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(3, false, "D+", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(3, true, "E-", 45.0f);
        ImGui::SameLine(0, 8.0f);
        
        render_note_button(4, false, "E", 45.0f);
        ImGui::SameLine(0, 8.0f);
        
        render_note_button(5, false, "F", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(6, false, "F+", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(6, true, "G-", 45.0f);
        
        // Row break
        ImGui::NewLine();
        ImGui::Spacing();
        
        // Row 2
        render_note_button(7, false, "G", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(8, false, "G+", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(8, true, "A-", 45.0f);
        ImGui::SameLine(0, 8.0f);
        
        render_note_button(9, false, "A", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(10, false, "A+", 45.0f);
        ImGui::SameLine(0, 3.0f);
        render_note_button(10, true, "B-", 45.0f);
        ImGui::SameLine(0, 8.0f);
        
        render_note_button(11, false, "B", 45.0f);
        
        // Remove the separate "Flats:" section - flats are now integrated above
        // Octave change options (gray buttons)
        ImGui::Spacing();
        ImGui::Text("Octave Change:");
        bool octave_lower_selected = (m_selected_octave_change == -1);
        bool octave_none_selected = (m_selected_octave_change == 0);
        bool octave_raise_selected = (m_selected_octave_change == 1);
        
        if (octave_lower_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        }
        if (ImGui::Button("Lower (<)", ImVec2(100, 30))) {
            m_selected_octave_change = -1;
        }
        if (octave_lower_selected) {
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 10.0f);
        
        if (octave_none_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        }
        if (ImGui::Button("None", ImVec2(100, 30))) {
            m_selected_octave_change = 0;
        }
        if (octave_none_selected) {
            ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0, 10.0f);
        
        if (octave_raise_selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
        }
        if (ImGui::Button("Raise (>)", ImVec2(100, 30))) {
            m_selected_octave_change = 1;
        }
        if (octave_raise_selected) {
            ImGui::PopStyleColor(3);
        }
        
        ImGui::Separator();
        
        // Pattern buttons
        ImGui::Text("Pattern Steps (click to apply selected note):");
        ImGui::BeginChild("PatternButtons", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        float button_width = 40.0f;
        float button_height = 30.0f;
        const int buttons_per_row = 8; // Group buttons in rows of 8
        
        // Recalculate total_steps in case it changed
        total_steps = GetTotalSteps();
        // Ensure m_is_flat is the same size as m_pattern
        if (m_is_flat.size() != total_steps) {
            m_is_flat.resize(total_steps, false);
        }
        for (int i = 0; i < total_steps; ++i) {
            // Start a new row every 8 buttons
            if (i > 0 && (i % buttons_per_row) != 0) {
                ImGui::SameLine();
            }
            
            int note = m_pattern[i];
            std::string button_label;
            if (note == -2) {
                button_label = "^";
            } else if (note < 0) {
                button_label = "R";
            } else if (note < NOTE_COUNT) {
                // Ensure m_is_flat vector is in bounds and properly sized
                if (i >= m_is_flat.size()) {
                    m_is_flat.resize(i + 1, false);
                }
                bool is_flat = m_is_flat[i];
                
                // Use the label directly from NOTE_NAMES, unless it's explicitly a flat
                if (is_flat) {
                    // Show flat names when explicitly selected as flat (using - notation)
                    if (note == 1) button_label = "D-";
                    else if (note == 3) button_label = "E-";
                    else if (note == 6) button_label = "G-";
                    else if (note == 8) button_label = "A-";
                    else if (note == 10) button_label = "B-";
                    else button_label = NOTE_NAMES[note];
                } else {
                    // Use the label directly from NOTE_NAMES (includes sharps like "C+", "D+", "F+", etc.)
                    button_label = NOTE_NAMES[note];
                }
            } else {
                button_label = "?";
            }
            
            // Use PushID to set button ID separately, avoiding conflicts with # in label
            ImGui::PushID(i);
            
            // Style the button based on whether it's a rest, tie, or a note
            if (note == -2) {
                // Tie - use a different color (blue-ish)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.3f, 0.5f, 1.0f));
            } else if (note < 0) {
                // Rest - gray
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            } else {
                // Note - match color to selection (base / sharp / flat)
                bool is_flat = (i < m_is_flat.size()) ? m_is_flat[i] : false;
                ImVec4 note_color = GetNoteColor(note, is_flat);
                ImVec4 hover_color = ImVec4(
                    std::min(1.0f, note_color.x * 1.2f),
                    std::min(1.0f, note_color.y * 1.2f),
                    std::min(1.0f, note_color.z * 1.2f),
                    1.0f
                );
                ImVec4 active_color = ImVec4(
                    note_color.x * 0.8f,
                    note_color.y * 0.8f,
                    note_color.z * 0.8f,
                    1.0f
                );
                ImGui::PushStyleColor(ImGuiCol_Button, note_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, active_color);
            }
            
            // Add octave indicator to button label if present
            int octave_change = m_octave_changes[i];
            std::string display_label = button_label;
            if (octave_change == -1) {
                display_label = "<" + display_label;
            } else if (octave_change == 1) {
                display_label = ">" + display_label;
            }
            
            if (ImGui::Button(display_label.c_str(), ImVec2(button_width, button_height))) {
                // Apply the currently selected note/option to this pattern step
                // Ensure m_is_flat and m_octave_changes are properly sized
                if (i >= m_is_flat.size()) {
                    m_is_flat.resize(i + 1, false);
                }
                if (i >= m_octave_changes.size()) {
                    m_octave_changes.resize(i + 1, 0);
                }
                
                // Apply selected note
                m_pattern[i] = m_selected_note;
                m_is_flat[i] = m_selected_note_is_flat;
                m_octave_changes[i] = m_selected_octave_change;
                
                UpdateMML();
            }
            
            ImGui::PopStyleColor(3);
            
            ImGui::PopID();
            
            // Show step number and current note as tooltip
            if (ImGui::IsItemHovered()) {
                std::string tooltip = "Step " + std::to_string(i + 1) + ": ";
                if (octave_change == -1) {
                    tooltip += "< ";
                } else if (octave_change == 1) {
                    tooltip += "> ";
                }
                if (note == -2) {
                    tooltip += "Tie (^)";
                } else if (note < 0) {
                    tooltip += "Rest";
                } else {
                    tooltip += NOTE_NAMES[note];
                }
                ImGui::SetTooltip("%s", tooltip.c_str());
            }
        }
        
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // MML output text box
        ImGui::Text("MML Output:");
        ImGui::InputTextMultiline("##MMLOutput", m_mml_buffer.data(), 
                                  m_mml_buffer.size(), 
                                  ImVec2(-1, 180), 
                                  ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
        
        // Add a copy button for convenience (underneath the text box)
        if (ImGui::Button("Copy MML to clipboard")) {
            ImGui::SetClipboardText(m_mml_output.c_str());
        }
        
        // Help text
        //ImGui::Separator();
        //ImGui::TextWrapped("Click pattern buttons to open a menu and select a note. Right-click or click outside to close the menu.");
    }
    ImGui::End();
}

