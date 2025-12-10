// UI for exporting the embedded mdsdrv.bin payload.
#include "mdsbin_export_window.h"

#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <cstring>

#include "mdsdrv_bin.h"

namespace fs = std::filesystem;

MDSBinExportWindow::MDSBinExportWindow()
    : m_open(false),
      m_request_focus(false),
      m_browse_save(false),
      m_fs(true, false, true) {
    std::memset(m_output_path, 0, sizeof(m_output_path));
    std::strncpy(m_output_path, "mdsdrv.bin", sizeof(m_output_path) - 1);
    m_status_message = "Ready";
}

void MDSBinExportWindow::Render() {
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);

    if (m_request_focus) {
        ImGui::SetNextWindowFocus();
        m_request_focus = false;
    }

    if (ImGui::Begin("mdsdrv.bin export", &m_open)) {
        ImGui::TextWrapped("Export the embedded MDSDRV driver binary to disk.");
        ImGui::Separator();

        ImGui::InputText("Destination", m_output_path, sizeof(m_output_path));
        ImGui::SameLine();
        bool trigger_save = ImGui::Button("Browse...");

        if (ImGui::Button("Save mdsdrv.bin")) {
            SaveBinary(m_output_path);
        }

        if (m_browse_save) {
            ImVec2 size(520, 380);
            ImVec2 center = ImGui::GetIO().DisplaySize * 0.5f;
            ImVec2 pos(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

            const char* path = m_fs.saveFileDialog(trigger_save, m_output_path, "mdsdrv.bin", ".bin", "Save mdsdrv.bin", size, pos);
            if (std::strlen(path) > 0) {
                std::strncpy(m_output_path, path, sizeof(m_output_path) - 1);
                SaveBinary(m_output_path);
                m_browse_save = false;
            } else if (m_fs.hasUserJustCancelledDialog()) {
                m_browse_save = false;
            }
        }
        else if (trigger_save) {
            // Start the dialog on this frame
            m_browse_save = true;
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", m_status_message.c_str());
    }
    ImGui::End();
}

void MDSBinExportWindow::SaveBinary(const char* path) {
    if (!path || std::strlen(path) == 0) {
        m_status_message = "Please choose a destination file.";
        return;
    }
    try {
        fs::path out_path(path);
        if (out_path.has_parent_path()) {
            fs::create_directories(out_path.parent_path());
        }
        std::ofstream out(path, std::ios::binary);
        if (!out) {
            m_status_message = std::string("Failed to open for write: ") + path;
            return;
        }
        out.write(reinterpret_cast<const char*>(g_mdsdrv_bin), static_cast<std::streamsize>(g_mdsdrv_bin_size));
        if (!out) {
            m_status_message = std::string("Write failed: ") + path;
            return;
        }
        m_status_message = std::string("Wrote ") + std::to_string(g_mdsdrv_bin_size) + " bytes to " + path;
    } catch (const std::exception& e) {
        m_status_message = std::string("Error: ") + e.what();
    }
}
