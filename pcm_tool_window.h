#ifndef PCM_TOOL_WINDOW_H
#define PCM_TOOL_WINDOW_H

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "imguifilesystem.h"

// Forward declarations
class Audio_Stream;

class PCMToolWindow {
public:
    PCMToolWindow();
    ~PCMToolWindow();
    
    void Render();
    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; if (open) m_request_focus = true; }
    void LoadPCMData(const std::vector<short>& data, int rate, int ch, const std::string& name = "");
    
    // Callback type for creating new windows
    typedef std::function<void(std::shared_ptr<PCMToolWindow>)> CreateWindowCallback;
    static void SetCreateWindowCallback(CreateWindowCallback callback);

private:
    void ExportToNewWindow();
    void LoadFile(const char* filename);
    void SaveFile(const char* filename);
    void ResampleAndSave(const char* filename);
    void ResampleAndSaveSlices(const char* base_filename);
    void StartPreview();
    void StopPreview();

    ImGuiFs::Dialog m_fs;
    bool m_browse_open;
    bool m_browse_save;
    char m_input_path[1024];

    std::vector<short> m_pcm_data;
    int m_sample_rate;
    int m_channels;
    int m_start_point;
    int m_end_point;
    
    bool m_preview_loop;
    bool m_double_speed;
    std::shared_ptr<Audio_Stream> m_preview_stream;
    int m_current_playback_position;
    
    // Slicing options
    bool m_slice_enabled;
    int m_num_slices;
    
    // Zoom state
    bool m_zoom_enabled;
    int m_zoom_point;
    float m_zoom_level;
    int m_zoom_window_samples;

    std::string m_status_message;
    std::string m_current_filename;
    
    bool m_open;
    bool m_request_focus;
    uint32_t m_id;  // Unique ID for this window instance
    
    // Static callback for creating new windows
    static CreateWindowCallback s_create_window_callback;
    static uint32_t s_id_counter;  // Counter for unique IDs
    
    // Waveform visualization helper
    static float WaveformGetter(void* data, int idx);
};

#endif // PCM_TOOL_WINDOW_H
