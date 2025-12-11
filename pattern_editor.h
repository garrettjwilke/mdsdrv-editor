#ifndef PATTERN_EDITOR_H
#define PATTERN_EDITOR_H

#include <vector>
#include <string>

class PatternEditor {
public:
    PatternEditor();
    ~PatternEditor();
    
    void Render();
    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; if (open) m_request_focus = true; }
    void SetEditorText(const std::string& text);
    std::string GetModifiedEditorText() const { return m_modified_editor_text; }
    bool HasUnsavedChanges() const { return m_has_unsaved_changes; }

private:
    void UpdateMML();
    std::string NoteToMML(int note_index, int note_length);
    
    int m_pattern_length;  // Number of bars in the pattern
    int m_note_length;      // Note length (1, 2, 4, 8, 16, 32)
    int m_instrument;       // Instrument number (-1 = none, 1+ = instrument number)
    bool m_is_drum_track;   // Whether this is a drum track (D) or instrument (@)
    int m_octave;           // Starting octave (-1 = none, 2-9 = octave number)
    std::vector<int> m_pattern;  // Pattern data: -2 = tie, -1 = rest, 0-11 = C to B
    std::vector<bool> m_is_flat;  // Whether each note is flat (true) or sharp (false), only used for notes that can be either
    std::vector<int> m_octave_changes;  // Octave changes: -1 = lower (<), 0 = none, 1 = raise (>)
    int m_selected_note;  // Currently selected note for pattern editing (-2 = tie, -1 = rest, 0-11 = notes)
    bool m_selected_note_is_flat;  // Whether the selected note is flat (for notes that can be either)
    int m_selected_octave_change;  // Selected octave change (-1 = lower, 0 = none, 1 = raise)
    
    int GetStepsPerBar() const;  // Calculate steps per bar based on note length
    int GetTotalSteps() const;    // Calculate total number of steps (bars × steps per bar)
    
    std::string m_mml_output;
    std::vector<char> m_mml_buffer;
    std::vector<char> m_pattern_name_buffer;  // Buffer for pattern name input
    std::string m_editor_text;  // Current editor text for pattern scanning
    std::string m_modified_editor_text;  // Modified editor text with pattern changes
    int m_selected_pattern_macro;  // Currently selected pattern macro number (-1 if none)
    bool m_has_unsaved_changes;  // Whether there are unsaved changes
    std::string m_pattern_name;  // Name of the currently selected pattern
    
    bool m_open;
    bool m_request_focus;
    
    void ApplyPatternChanges();  // Apply changes to the editor text
    void CancelPatternChanges();  // Cancel changes and reload original pattern
    void CreateDefaultPattern();  // Create a default *701 pattern if none exists
    void CreateNewPattern(bool copy_current);  // Create a new pattern (copy current or clean)
    int FindNextAvailableMacro();  // Find the next available macro number (701-799)
    
    // Pattern scanning and loading
    struct PatternInfo {
        std::string content;
        int bars;
        int note_length;
        int instrument;
        bool is_drum_track;  // Whether this is a drum track (D) or instrument (@)
        int octave;
        int macro_number;  // The macro number (701, 702, etc.) - pattern number is macro_number - 700
        std::string name;  // Optional pattern name (after semicolon)
    };
    std::vector<PatternInfo> ScanForPatterns(const std::string& text);
    bool LoadPattern(const PatternInfo& pattern);
    
    // Note names for display
    static const char* NOTE_NAMES[];
    static const int NOTE_COUNT;
};

#endif // PATTERN_EDITOR_H

