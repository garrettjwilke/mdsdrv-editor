// Simple window for exporting the embedded mdsdrv.bin driver.
#pragma once

#include <string>
#include "imguifilesystem.h"

class MDSBinExportWindow {
public:
    MDSBinExportWindow();

    void Render();
    bool IsOpen() const { return m_open; }
    void SetOpen(bool open) { m_open = open; if (open) m_request_focus = true; }

private:
    void SaveBinary(const char* path);

    bool m_open;
    bool m_request_focus;
    bool m_browse_save;
    char m_output_path[1024];
    std::string m_status_message;

    ImGuiFs::Dialog m_fs;
};
