#ifndef EXPORT_WINDOW_H
#define EXPORT_WINDOW_H

#include <string>
#include "imguifilesystem.h"

class ExportWindow {
public:
    ExportWindow();
    
    void Render();
    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; if (open) m_request_focus = true; }

private:
    char m_bgm_path[1024];
    char m_sfx_path[1024];
    char m_output_path[1024];
    char m_seq_filename[256];
    char m_pcm_filename[256];
    char m_header_filename[256];
    
    std::string m_status_message;
    
    bool m_open;
    bool m_browse_bgm;
    bool m_browse_sfx;
    bool m_browse_output;
    bool m_request_focus;
    
    ImGuiFs::Dialog m_fs;
    
    void RunExport();
};

#endif // EXPORT_WINDOW_H
