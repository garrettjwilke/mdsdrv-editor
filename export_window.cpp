#include "export_window.h"
#include <imgui.h>
#include "platform/mdsdrv.h"
#include "song.h"
#include "mml_input.h"
#include "riff.h"
#include "stringf.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

ExportWindow::ExportWindow() : m_fs(true, false, true), m_open(false),
                               m_browse_bgm(false), m_browse_sfx(false), m_browse_output(false)
{
    std::string cwd = fs::current_path().string();
    strncpy(m_bgm_path, "musicdata", sizeof(m_bgm_path) - 1);
    strncpy(m_sfx_path, "sfxdata", sizeof(m_sfx_path) - 1);
    strncpy(m_output_path, cwd.c_str(), sizeof(m_output_path) - 1);
    strncpy(m_seq_filename, "mdsseq.bin", sizeof(m_seq_filename) - 1);
    strncpy(m_pcm_filename, "mdspcm.bin", sizeof(m_pcm_filename) - 1);
    strncpy(m_header_filename, "mdsseq.h", sizeof(m_header_filename) - 1);
    m_status_message = "Ready";
}

void ExportWindow::Render()
{
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("mdslink export", &m_open))
    {
        ImGui::InputText("BGM MML Directory", m_bgm_path, sizeof(m_bgm_path));
        ImGui::SameLine();
        bool trigger_bgm = ImGui::Button("...##bgm");

        ImGui::InputText("SFX MML Directory", m_sfx_path, sizeof(m_sfx_path));
        ImGui::SameLine();
        bool trigger_sfx = ImGui::Button("...##sfx");

        ImGui::InputText("Output Directory", m_output_path, sizeof(m_output_path));
        ImGui::SameLine();
        bool trigger_output = ImGui::Button("...##output");
        
        if (trigger_bgm) {
            m_browse_bgm = true;
            m_browse_sfx = false;
            m_browse_output = false;
        }
        if (trigger_sfx) {
            m_browse_bgm = false;
            m_browse_sfx = true;
            m_browse_output = false;
        }
        if (trigger_output) {
            m_browse_bgm = false;
            m_browse_sfx = false;
            m_browse_output = true;
        }

        if (m_browse_bgm) {
            const char* path = m_fs.chooseFolderDialog(trigger_bgm, m_bgm_path);
            if (strlen(path) > 0) {
                strncpy(m_bgm_path, path, sizeof(m_bgm_path) - 1);
                m_browse_bgm = false;
            } else if (m_fs.hasUserJustCancelledDialog()) {
                m_browse_bgm = false;
            }
        }
        else if (m_browse_sfx) {
            const char* path = m_fs.chooseFolderDialog(trigger_sfx, m_sfx_path);
            if (strlen(path) > 0) {
                strncpy(m_sfx_path, path, sizeof(m_sfx_path) - 1);
                m_browse_sfx = false;
            } else if (m_fs.hasUserJustCancelledDialog()) {
                m_browse_sfx = false;
            }
        }
        else if (m_browse_output) {
            const char* path = m_fs.chooseFolderDialog(trigger_output, m_output_path);
            if (strlen(path) > 0) {
                strncpy(m_output_path, path, sizeof(m_output_path) - 1);
                m_browse_output = false;
            } else if (m_fs.hasUserJustCancelledDialog()) {
                m_browse_output = false;
            }
        }

        ImGui::Separator();
        ImGui::InputText("Sequence Filename", m_seq_filename, sizeof(m_seq_filename));
        ImGui::InputText("PCM Filename", m_pcm_filename, sizeof(m_pcm_filename));
        ImGui::InputText("Header Filename", m_header_filename, sizeof(m_header_filename));
        ImGui::Separator();
        
        if (ImGui::Button("Export"))
        {
            RunExport();
        }
        
        ImGui::Separator();
        ImGui::Text("Output:");
        ImGui::BeginChild("export_output", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(m_status_message.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}

static Song convert_file(const std::string& filename, std::string& log_output)
{
    Song song;
    MML_Input input = MML_Input(&song);
    input.open_file(filename.c_str());
    return song;
}

void ExportWindow::RunExport()
{
    m_status_message = "Exporting...";
    std::vector<std::string> input_files;
    
    try {
        // Search BGM directory
        if (fs::exists(m_bgm_path) && fs::is_directory(m_bgm_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(m_bgm_path)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().string();
                    std::string ext = entry.path().extension().string();
                    // Check for .mml or .mds extension (case insensitive)
                    if (iequal(ext, ".mml") || iequal(ext, ".mds")) {
                        input_files.push_back(path);
                    }
                }
            }
        } else if (strlen(m_bgm_path) > 0) {
            m_status_message = "Invalid BGM directory: " + std::string(m_bgm_path);
            return;
        }

        // Search SFX directory
        if (fs::exists(m_sfx_path) && fs::is_directory(m_sfx_path)) {
            for (const auto& entry : fs::recursive_directory_iterator(m_sfx_path)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().string();
                    std::string ext = entry.path().extension().string();
                    // Check for .mml or .mds extension (case insensitive)
                    if (iequal(ext, ".mml") || iequal(ext, ".mds")) {
                        input_files.push_back(path);
                    }
                }
            }
        } else if (strlen(m_sfx_path) > 0) {
            m_status_message = "Invalid SFX directory: " + std::string(m_sfx_path);
            return;
        }
        
        if (input_files.empty()) {
            m_status_message = "No .mml or .mds files found in BGM or SFX directories.";
            return;
        }

        MDSDRV_Linker linker;
        std::string log;
        log = "Processing " + std::to_string(input_files.size()) + " file(s)...\n\n";

        for (size_t i = 0; i < input_files.size(); ++i) {
            const auto& file = input_files[i];
            std::string ext = fs::path(file).extension().string();
            std::string filename_stem = fs::path(file).stem().string();
            
            log += "[" + std::to_string(i+1) + "/" + std::to_string(input_files.size()) + "] " + file + "\n";

            RIFF mds(0);
            
            if (iequal(ext, ".mds")) {
                std::ifstream in(file, std::ios::binary | std::ios::ate);
                if (in) {
                    auto size = in.tellg();
                    std::vector<uint8_t> data(size);
                    in.seekg(0);
                    if (in.read((char*)data.data(), size)) {
                        mds = RIFF(data);
                    } else {
                        m_status_message = "Failed to read " + file;
                        return;
                    }
                } else {
                    m_status_message = "Failed to open " + file;
                    return;
                }
            } else {
                // MML
                Song song = convert_file(file, log);
                MDSDRV_Converter converter(song);
                mds = converter.get_mds();
            }
            
            linker.add_song(mds, filename_stem);
        }
        
        log += "\n";

        fs::path out_dir(m_output_path);
        if (!fs::exists(out_dir)) {
            fs::create_directories(out_dir);
        }

        // Write seq
        if (strlen(m_seq_filename) > 0) {
            fs::path p = out_dir / m_seq_filename;
            log += "Writing " + p.string() + "...\n";
            auto bytes = linker.get_seq_data();
            std::ofstream out(p, std::ios::binary);
            out.write((char*)bytes.data(), bytes.size());
            log += "  Wrote " + std::to_string(bytes.size()) + " bytes\n";
        }

        // Write pcm
        if (strlen(m_pcm_filename) > 0) {
            fs::path p = out_dir / m_pcm_filename;
            log += "Writing " + p.string() + "...\n";
            auto bytes = linker.get_pcm_data();
            std::ofstream out(p, std::ios::binary);
            out.write((char*)bytes.data(), bytes.size());
            log += "  Wrote " + std::to_string(bytes.size()) + " bytes\n";
            log += "\n" + linker.get_statistics();
        }

        // Write header
        if (strlen(m_header_filename) > 0) {
            fs::path p = out_dir / m_header_filename;
            log += "Writing " + p.string() + "...\n";
            std::string header_content = linker.get_c_header();
            std::ofstream out(p);
            out.write(header_content.c_str(), header_content.size());
            log += "  Wrote " + std::to_string(header_content.size()) + " bytes\n";
        }
        
        m_status_message = "Export Successful!\n\n" + log;

    } catch (const std::exception& e) {
        m_status_message = std::string("Error: ") + e.what();
    } catch (...) {
        m_status_message = "Unknown error occurred.";
    }
}
