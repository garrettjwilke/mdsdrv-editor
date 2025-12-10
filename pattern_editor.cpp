#include "pattern_editor.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>

const char* PatternEditor::NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

const int PatternEditor::NOTE_COUNT = 12;

PatternEditor::PatternEditor() 
    : m_pattern_length(16)
    , m_note_length(4)
    , m_open(false)
    , m_request_focus(false)
{
    m_pattern.resize(m_pattern_length, -1); // Initialize with rests
    m_octave_changes.resize(m_pattern_length, 0); // Initialize with no octave changes
    m_mml_buffer.resize(4096, 0); // Allocate buffer for MML output
    UpdateMML();
}

PatternEditor::~PatternEditor() {
}

void PatternEditor::UpdateMML() {
    std::ostringstream oss;
    
    // Output the default note length command
    oss << "l" << m_note_length;
    
    // Output all notes/rests without length suffixes
    for (int i = 0; i < m_pattern_length; ++i) {
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
            const char* note_chars[] = {
                "c", "c#", "d", "d#", "e", "f",
                "f#", "g", "g#", "a", "a#", "b"
            };
            oss << note_chars[note];
        } else {
            // Output a rest
            oss << "r";
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
        // Pattern length selector
        ImGui::Text("Pattern Length:");
        ImGui::SameLine();
        if (ImGui::InputInt("##PatternLength", &m_pattern_length, 1, 1)) {
            m_pattern_length = std::max(1, std::min(64, m_pattern_length));
            m_pattern.resize(m_pattern_length, -1);
            m_octave_changes.resize(m_pattern_length, 0);
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
            UpdateMML();
        }
        
        ImGui::Separator();
        
        // Pattern buttons
        ImGui::Text("Pattern Steps:");
        ImGui::BeginChild("PatternButtons", ImVec2(0, 200), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        float button_width = 40.0f;
        float button_height = 30.0f;
        const int buttons_per_row = 8; // Group buttons in rows of 8
        
        for (int i = 0; i < m_pattern_length; ++i) {
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
                button_label = NOTE_NAMES[note];
            } else {
                button_label = "?";
            }
            
            std::string button_id = "##Step" + std::to_string(i);
            std::string popup_id = "NotePopup" + std::to_string(i);
            
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
            
            if (ImGui::Button((display_label + button_id).c_str(), ImVec2(button_width, button_height))) {
                ImGui::OpenPopup(popup_id.c_str());
            }
            
            ImGui::PopStyleColor(3);
            
            // Popup menu for note selection
            if (ImGui::BeginPopup(popup_id.c_str())) {
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
                for (int n = 0; n < NOTE_COUNT; ++n) {
                    bool is_selected = (note == n);
                    std::string note_label = NOTE_NAMES[n];
                    if (ImGui::Selectable(note_label.c_str(), is_selected)) {
                        m_pattern[i] = n;
                        UpdateMML();
                        ImGui::CloseCurrentPopup();
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
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
        
        // Add a copy button for convenience
        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            ImGui::SetClipboardText(m_mml_output.c_str());
        }
        
        // Help text
        ImGui::Separator();
        ImGui::TextWrapped("Click pattern buttons to open a menu and select a note. Right-click or click outside to close the menu.");
    }
    ImGui::End();
}

