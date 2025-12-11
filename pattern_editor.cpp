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

PatternEditor::PatternEditor() 
    : m_pattern_length(1)
    , m_note_length(4)
    , m_instrument(-1)
    , m_octave(-1)
    , m_open(false)
    , m_request_focus(false)
{
    int total_steps = GetTotalSteps();
    m_pattern.resize(total_steps, -1); // Initialize with rests
    m_is_flat.resize(total_steps, false); // Initialize with sharps (false = sharp)
    m_octave_changes.resize(total_steps, 0); // Initialize with no octave changes
    m_mml_buffer.resize(4096, 0); // Allocate buffer for MML output
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

std::vector<PatternEditor::PatternInfo> PatternEditor::ScanForPatterns(const std::string& text) {
    std::vector<PatternInfo> patterns;
    
    // Find all pattern macros (*701 through *799)
    // Pattern format: *701 @1 o3 l4 a b c d | e f g a; (semicolon optional)
    // We'll find each *number and extract content until next *number or end
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
        
        // Find the content after this macro number
        size_t macro_pos = match.position() + match.length();
        std::string pattern_content;
        
        // Extract content until next *number or semicolon or end of string
        size_t content_start = macro_pos;
        while (content_start < text.length() && std::isspace(text[content_start])) {
            content_start++;
        }
        
        // Find the end: next *number at start of line, semicolon, or end
        size_t content_end = content_start;
        while (content_end < text.length()) {
            if (text[content_end] == ';') {
                break;
            }
            // Check if this is the start of a new macro (after newline or at start)
            if (content_end > 0 && text[content_end - 1] == '\n' && text[content_end] == '*') {
                // Check if it's followed by digits
                size_t check_pos = content_end + 1;
                while (check_pos < text.length() && std::isdigit(text[check_pos])) {
                    check_pos++;
                }
                if (check_pos > content_end + 1) {
                    // Found a new macro, stop here
                    break;
                }
            }
            content_end++;
        }
        
        // Extract the content
        if (content_end > content_start) {
            pattern_content = text.substr(content_start, content_end - content_start);
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
        info.octave = -1;
        info.note_length = 4; // default
        info.bars = 1; // default
        info.macro_number = macro_number; // Store the macro number
        
        // Parse the pattern content
        std::istringstream iss(cleaned_content);
        std::string token;
        
        while (iss >> token) {
            // Check for instrument @X
            if (token[0] == '@' && token.length() > 1) {
                try {
                    info.instrument = std::stoi(token.substr(1));
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
    // Set pattern length (bars)
    m_pattern_length = pattern.bars;
    
    // Set note length
    m_note_length = pattern.note_length;
    
    // Set instrument
    m_instrument = pattern.instrument;
    
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
        
        // Skip @X, oX, lX commands
        if (content[content_pos] == '@' || content[content_pos] == 'o' || content[content_pos] == 'l') {
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
    
    // Output instrument number if specified (must be >= 1)
    if (m_instrument >= 1) {
        oss << "@" << m_instrument << " ";
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
        if (!found_patterns.empty()) {
            ImGui::Text("Found Patterns:");
            if (ImGui::BeginListBox("##PatternList", ImVec2(-1, 100))) {
                for (size_t i = 0; i < found_patterns.size(); ++i) {
                    const auto& pat = found_patterns[i];
                    // Pattern number is macro_number - 700 (701 = pattern 1, 702 = pattern 2, etc.)
                    int pattern_number = pat.macro_number - 700;
                    std::string label = "Pattern " + std::to_string(pattern_number) + " (*" + std::to_string(pat.macro_number) + ")";
                    if (pat.instrument >= 1) {
                        label += " @" + std::to_string(pat.instrument);
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
            ImGui::Separator();
        }
        
        // Calculate total steps once at the start
        int total_steps = GetTotalSteps();
        
        // Pattern length selector (in bars)
        ImGui::Text("Pattern Length (bars):");
        ImGui::SameLine();
        if (ImGui::InputInt("##PatternLength", &m_pattern_length, 1, 1)) {
            m_pattern_length = std::max(1, std::min(16, m_pattern_length));
            total_steps = GetTotalSteps();
            m_pattern.resize(total_steps, -1);
            m_is_flat.resize(total_steps, false);
            m_octave_changes.resize(total_steps, 0);
            UpdateMML();
        }
        
        // Note length selector
        ImGui::Text("Note Length:");
        ImGui::SameLine();
        const char* note_length_names[] = { "1 (Whole)", "2 (Half)", "4 (Quarter)", "8 (Eighth)", "16 (Sixteenth)", "32 (Thirty-second)" };
        int note_length_values[] = { 1, 2, 4, 8, 16, 32 };
        int current_index = 0;
        for (int i = 0; i < 6; ++i) {
            if (note_length_values[i] == m_note_length) {
                current_index = i;
                break;
            }
        }
        if (ImGui::Combo("##NoteLength", &current_index, note_length_names, 6)) {
            m_note_length = note_length_values[current_index];
            // Resize pattern arrays when note length changes
            total_steps = GetTotalSteps();
            m_pattern.resize(total_steps, -1);
            m_is_flat.resize(total_steps, false);
            m_octave_changes.resize(total_steps, 0);
            UpdateMML();
        }
        
        // Instrument selector
        ImGui::Text("Instrument:");
        ImGui::SameLine();
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
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            int instrument_val = m_instrument;
            if (ImGui::InputInt("##InstrumentNumber", &instrument_val, 1, 1)) {
                m_instrument = std::max(1, instrument_val);
                UpdateMML();
            }
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("(none)");
        }
        
        // Octave selector
        ImGui::Text("Octave:");
        ImGui::SameLine();
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
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            int octave_val = m_octave;
            if (ImGui::InputInt("##OctaveNumber", &octave_val, 1, 1)) {
                m_octave = std::max(2, std::min(9, octave_val));
                UpdateMML();
            }
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("(none)");
        }
        
        ImGui::Separator();
        
        // Pattern buttons
        ImGui::Text("Pattern Steps:");
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
                // Note - green
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.2f, 1.0f));
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
                ImGui::OpenPopup("NotePopup");
            }
            
            ImGui::PopStyleColor(3);
            
            // Popup menu for note selection (must be within the same ID scope)
            if (ImGui::BeginPopup("NotePopup")) {
                // Rest option
                bool is_rest = (note == -1);
                if (ImGui::Selectable("Rest", is_rest)) {
                    m_pattern[i] = -1;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_rest) {
                    ImGui::SetItemDefaultFocus();
                }
                
                // Tie option
                bool is_tie = (note == -2);
                if (ImGui::Selectable("Tie (^)", is_tie)) {
                    m_pattern[i] = -2;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_tie) {
                    ImGui::SetItemDefaultFocus();
                }
                
                ImGui::Separator();
                
                // All note options
                // Ensure m_is_flat vector is properly sized
                if (i >= m_is_flat.size()) {
                    m_is_flat.resize(i + 1, false);
                }
                for (int n = 0; n < NOTE_COUNT; ++n) {
                    bool is_flat_val = m_is_flat[i];
                    bool is_selected = (note == n && !is_flat_val);
                    std::string note_label = NOTE_NAMES[n];
                    if (ImGui::Selectable(note_label.c_str(), is_selected)) {
                        // Ensure m_is_flat is large enough before setting values
                        if (i >= m_is_flat.size()) {
                            m_is_flat.resize(i + 1, false);
                        }
                        // Set the pattern note
                        m_pattern[i] = n;
                        // Set flat flag: only notes 1, 3, 6, 8, 10 can be flats
                        // When selected from regular menu, they're sharps (false)
                        if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10) {
                            m_is_flat[i] = false; // Sharp
                        } else {
                            // For natural notes, ensure flat flag is false (though it shouldn't matter)
                            m_is_flat[i] = false;
                        }
                        // Force immediate update
                        UpdateMML();
                        ImGui::CloseCurrentPopup();
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                
                // Flat alternatives for notes that can be flats
                ImGui::Separator();
                ImGui::Text("Flats:");
                // C+ can be D-
                bool is_db = (note == 1 && m_is_flat[i]);
                if (ImGui::Selectable("D-", is_db)) {
                    m_pattern[i] = 1; // C+/D-
                    m_is_flat[i] = true;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_db) {
                    ImGui::SetItemDefaultFocus();
                }
                // D+ can be E-
                bool is_eb = (note == 3 && m_is_flat[i]);
                if (ImGui::Selectable("E-", is_eb)) {
                    m_pattern[i] = 3; // D+/E-
                    m_is_flat[i] = true;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_eb) {
                    ImGui::SetItemDefaultFocus();
                }
                // F+ can be G-
                bool is_gb = (note == 6 && m_is_flat[i]);
                if (ImGui::Selectable("G-", is_gb)) {
                    m_pattern[i] = 6; // F+/G-
                    m_is_flat[i] = true;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_gb) {
                    ImGui::SetItemDefaultFocus();
                }
                // G+ can be A-
                bool is_ab = (note == 8 && m_is_flat[i]);
                if (ImGui::Selectable("A-", is_ab)) {
                    m_pattern[i] = 8; // G+/A-
                    m_is_flat[i] = true;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_ab) {
                    ImGui::SetItemDefaultFocus();
                }
                // A+ can be B-
                bool is_bb = (note == 10 && m_is_flat[i]);
                if (ImGui::Selectable("B-", is_bb)) {
                    m_pattern[i] = 10; // A+/B-
                    m_is_flat[i] = true;
                    UpdateMML();
                    ImGui::CloseCurrentPopup();
                }
                if (is_bb) {
                    ImGui::SetItemDefaultFocus();
                }
                
                ImGui::Separator();
                
                // Octave change options
                ImGui::Text("Octave:");
                bool octave_lower = (octave_change == -1);
                bool octave_none = (octave_change == 0);
                bool octave_raise = (octave_change == 1);
                
                if (ImGui::Selectable("Lower (<)", octave_lower)) {
                    m_octave_changes[i] = -1;
                    UpdateMML();
                }
                if (ImGui::Selectable("None", octave_none)) {
                    m_octave_changes[i] = 0;
                    UpdateMML();
                }
                if (ImGui::Selectable("Raise (>)", octave_raise)) {
                    m_octave_changes[i] = 1;
                    UpdateMML();
                }
                
                ImGui::EndPopup();
            }
            
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
                                  ImVec2(-1, 100), 
                                  ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
        
        // Add a copy button for convenience (underneath the text box)
        if (ImGui::Button("Copy")) {
            ImGui::SetClipboardText(m_mml_output.c_str());
        }
        
        // Help text
        ImGui::Separator();
        ImGui::TextWrapped("Click pattern buttons to open a menu and select a note. Right-click or click outside to close the menu.");
    }
    ImGui::End();
}

