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

private:
    void UpdateMML();
    std::string NoteToMML(int note_index, int note_length);
    
    int m_pattern_length;  // Number of bars in the pattern
    int m_note_length;      // Note length (1, 2, 4, 8, 16, 32)
    int m_instrument;       // Instrument number (-1 = none, 1+ = instrument number)
    int m_octave;           // Starting octave (-1 = none, 2-9 = octave number)
    std::vector<int> m_pattern;  // Pattern data: -2 = tie, -1 = rest, 0-11 = C to B
    std::vector<int> m_octave_changes;  // Octave changes: -1 = lower (<), 0 = none, 1 = raise (>)
    
    int GetStepsPerBar() const;  // Calculate steps per bar based on note length
    int GetTotalSteps() const;    // Calculate total number of steps (bars × steps per bar)
    
    std::string m_mml_output;
    std::vector<char> m_mml_buffer;
    
    bool m_open;
    bool m_request_focus;
    
    // Note names for display
    static const char* NOTE_NAMES[];
    static const int NOTE_COUNT;
};

#endif // PATTERN_EDITOR_H

