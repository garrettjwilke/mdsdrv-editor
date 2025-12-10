#include "pcm_tool_window.h"
#include <imgui.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <cstdint>
#include <filesystem>
#include "stringf.h"
#include "audio_manager.h"

namespace fs = std::filesystem;

// Static callback initialization
PCMToolWindow::CreateWindowCallback PCMToolWindow::s_create_window_callback = nullptr;
uint32_t PCMToolWindow::s_id_counter = 0;

// Simple Audio Stream for Preview
class PCM_Preview_Stream : public Audio_Stream
{
public:
    PCM_Preview_Stream(const std::vector<short>& data, int start, int end, int rate, bool loop, int* position_ptr)
        : data(data), start(start), end(end), rate(rate), loop(loop), pos(0.0), step(0.0), position_ptr(position_ptr)
    {
        if (this->start < 0) this->start = 0;
        if (this->end > (int)this->data.size()) this->end = (int)this->data.size();
        if (this->start >= this->end) {
             this->start = 0;
             this->end = 0;
        }
    }

    void setup_stream(uint32_t output_rate) override
    {
        if (output_rate > 0 && rate > 0)
            step = (double)rate / (double)output_rate;
        else
            step = 1.0;
        pos = 0.0;
    }

    int get_sample(WAVE_32BS* output, int count, int channels) override
    {
        if (start >= end) {
            for (int i = 0; i < count; ++i) {
                output[i].L = 0;
                output[i].R = 0;
            }
            if (!finished) set_finished(true);
            return 0;
        }

        for (int i = 0; i < count; ++i)
        {
            if (position_ptr) {
                int current_idx = start + (int)pos;
                if (loop && current_idx >= end) {
                    current_idx = start + ((current_idx - start) % (end - start));
                }
                if (current_idx < start) current_idx = start;
                if (current_idx > end) current_idx = end;
                *position_ptr = current_idx;
            }
            
            int idx0 = start + (int)pos;
            int idx1 = idx0 + 1;
            
            if (idx0 >= end)
            {
                if (loop)
                {
                     pos -= (end - start);
                     idx0 = start + (int)pos;
                     idx1 = idx0 + 1;
                }
                else
                {
                    for (; i < count; ++i) {
                        output[i].L = 0;
                        output[i].R = 0;
                    }
                    if (!finished) set_finished(true);
                    return 0;
                }
            }
            
            if (idx1 >= end)
            {
                 if (loop) idx1 = start; 
                 else idx1 = end - 1;
            }

            if (idx0 < 0) idx0 = 0;
            if (idx1 < 0) idx1 = 0;
            if (idx0 >= (int)data.size()) idx0 = (int)data.size() - 1;
            if (idx1 >= (int)data.size()) idx1 = (int)data.size() - 1;

            double frac = pos - (int)pos;
            short s0 = data[idx0];
            short s1 = data[idx1];
            
            int32_t val = (int32_t)(s0 + (s1 - s0) * frac);

            output[i].L = val << 8;
            output[i].R = val << 8; 

            pos += step;
        }
        
        return 1;
    }

    void stop_stream() override
    {
    }

private:
    std::vector<short> data;
    int start;
    int end;
    int rate;
    bool loop;
    double pos;
    double step;
    int* position_ptr;
};

PCMToolWindow::PCMToolWindow() : m_fs(true, false, true), m_browse_open(false), m_browse_save(false),
                                  m_open(false)
{
    m_id = s_id_counter++;
    m_sample_rate = 0;
    m_channels = 0;
    m_start_point = 0;
    m_end_point = 0;
    m_preview_loop = false;
    m_double_speed = false;
    m_current_playback_position = -1;
    m_zoom_enabled = false;
    m_zoom_point = 0;
    m_zoom_level = 1.0f;
    m_zoom_window_samples = 1000;
    m_slice_enabled = false;
    m_num_slices = 2;
    m_status_message = "Ready";
    memset(m_input_path, 0, sizeof(m_input_path));
    m_request_focus = false;
}

PCMToolWindow::~PCMToolWindow()
{
    StopPreview();
}

float PCMToolWindow::WaveformGetter(void* data, int idx)
{
    const std::vector<short>* pcm = (const std::vector<short>*)data;
    if (idx < 0 || idx >= (int)pcm->size()) return 0.0f;
    return (float)(*pcm)[idx] / 32768.0f;
}

void PCMToolWindow::Render()
{
    if (!m_open) return;

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    
    // Bring window to front if focus was requested
    if (m_request_focus) {
        ImGui::SetNextWindowFocus();
        m_request_focus = false;
    }
    
    std::string window_title = "PCM Tool";
    if (!m_current_filename.empty()) {
        // Extract just the filename from the path
        std::string display_name = m_current_filename;
        try {
            fs::path file_path(m_current_filename);
            display_name = file_path.filename().string();
        } catch (...) {
            // If filesystem path parsing fails, try manual extraction
            size_t last_sep = m_current_filename.find_last_of("/\\");
            if (last_sep != std::string::npos && last_sep + 1 < m_current_filename.length()) {
                display_name = m_current_filename.substr(last_sep + 1);
            }
        }
        window_title += " - " + display_name;
    }
    
    // Use a unique ID for the window based on instance ID to avoid conflicts
    std::string window_id = window_title + "###PCMToolWindow" + std::to_string(m_id);
    
    bool window_open = m_open;
    if (ImGui::Begin(window_id.c_str(), &window_open))
    {
        m_open = window_open;
        
        bool load_clicked = ImGui::Button("Load WAV...");
        if (load_clicked)
        {
            m_browse_open = true;
            m_browse_save = false;
        }
        ImGui::SameLine();
        ImGui::Text("%s", m_current_filename.c_str());

        if (m_browse_open)
        {
            ImVec2 center = ImGui::GetIO().DisplaySize;
            center.x *= 0.5f;
            center.y *= 0.5f;
            ImVec2 size(600, 400);
            ImVec2 pos = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

            const char* path = m_fs.chooseFileDialog(load_clicked, m_input_path, ".wav;.mp3", "Load Audio", size, pos);
            if (strlen(path) > 0)
            {
                LoadFile(path);
                m_browse_open = false;
            }
            else if (m_fs.hasUserJustCancelledDialog())
            {
                m_browse_open = false;
            }
        }

        if (m_pcm_data.size() > 0)
        {
            ImGui::Separator();
            ImGui::Text("Sample Rate: %d Hz", m_sample_rate);
            ImGui::SameLine();
            ImGui::Text("Channels: %d", m_channels);
            ImGui::SameLine();
            ImGui::Text("Length: %d samples", (int)m_pcm_data.size());

            // Zoom controls
            ImGui::Checkbox("Zoom", &m_zoom_enabled);
            if (m_zoom_enabled) {
                ImGui::SameLine();
                if (ImGui::RadioButton("Start", m_zoom_point == 0)) m_zoom_point = 0;
                ImGui::SameLine();
                if (ImGui::RadioButton("End", m_zoom_point == 1)) m_zoom_point = 1;
                ImGui::SameLine();
                if (ImGui::Button("Zoom In")) {
                    m_zoom_window_samples = (int)(m_zoom_window_samples * 0.5f);
                    if (m_zoom_window_samples < 10) m_zoom_window_samples = 10;
                }
                ImGui::SameLine();
                if (ImGui::Button("Zoom Out")) {
                    m_zoom_window_samples = (int)(m_zoom_window_samples * 2.0f);
                    if (m_zoom_window_samples > (int)m_pcm_data.size()) m_zoom_window_samples = (int)m_pcm_data.size();
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset")) {
                    m_zoom_window_samples = 1000;
                }
            }

            // Waveform display
            float plot_height = 150.0f;
            ImVec2 content_region = ImGui::GetContentRegionAvail();
            float plot_width = content_region.x;
            
            float margin_x = 15.0f;
            float margin_y = 20.0f;
            
            ImVec2 box_min = ImGui::GetCursorScreenPos();
            box_min.x += margin_x;
            box_min.y += margin_y;
            ImVec2 box_max = ImVec2(box_min.x + plot_width - margin_x * 2.0f, box_min.y + plot_height);
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRect(box_min, box_max, IM_COL32(200, 200, 200, 255), 0.0f, 0, 1.0f);
            
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + margin_x);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + margin_y);
            
            ImVec2 plot_min = ImVec2(box_min.x + 2.0f, box_min.y);
            ImVec2 plot_max = ImVec2(box_max.x - 2.0f, box_max.y);
            
            // Calculate zoom window if enabled
            int zoom_start_sample = 0;
            int zoom_end_sample = (int)m_pcm_data.size();
            std::vector<short> zoom_data;
            
            if (m_zoom_enabled && m_pcm_data.size() > 0) {
                int zoom_center_sample = (m_zoom_point == 0) ? m_start_point : m_end_point;
                int half_window = m_zoom_window_samples / 2;
                zoom_start_sample = zoom_center_sample - half_window;
                zoom_end_sample = zoom_center_sample + half_window;
                
                if (zoom_start_sample < 0) {
                    zoom_end_sample += -zoom_start_sample;
                    zoom_start_sample = 0;
                }
                if (zoom_end_sample > (int)m_pcm_data.size()) {
                    zoom_start_sample -= (zoom_end_sample - (int)m_pcm_data.size());
                    zoom_end_sample = (int)m_pcm_data.size();
                    if (zoom_start_sample < 0) zoom_start_sample = 0;
                }
                
                zoom_data.clear();
                for (int i = zoom_start_sample; i < zoom_end_sample; ++i) {
                    if (i >= 0 && i < (int)m_pcm_data.size()) {
                        zoom_data.push_back(m_pcm_data[i]);
                    }
                }
            }
            
            ImVec2 plot_size = ImVec2(plot_width - margin_x * 2.0f, plot_height);
            std::string waveform_id = "##Waveform_" + std::to_string(m_id);
            if (m_zoom_enabled && !zoom_data.empty()) {
                ImGui::PlotLines(waveform_id.c_str(), WaveformGetter, (void*)&zoom_data, (int)zoom_data.size(), 0, NULL, -1.0f, 1.0f, plot_size);
            } else {
                ImGui::PlotLines(waveform_id.c_str(), WaveformGetter, (void*)&m_pcm_data, (int)m_pcm_data.size(), 0, NULL, -1.0f, 1.0f, plot_size);
            }
            
            bool selection_changed = false;

            if (m_pcm_data.size() > 0)
            {
                float width = plot_max.x - plot_min.x;
                float x_step;
                float count;
                
                if (m_zoom_enabled && !zoom_data.empty()) {
                    count = (float)(zoom_data.size() > 1 ? zoom_data.size() : 1);
                    x_step = width / count;
                } else {
                    count = (float)(m_pcm_data.size() > 1 ? m_pcm_data.size() : 1);
                    x_step = width / count;
                }
                
                float handle_size = 10.0f;
                
                auto handle_tab = [&](int* point, bool is_top, ImU32 color, const char* id) {
                    if (*point < 0) *point = 0;
                    if (*point > (int)m_pcm_data.size()) *point = (int)m_pcm_data.size();
                    
                    float x;
                    if (m_zoom_enabled && !zoom_data.empty()) {
                        int point_in_zoom = *point - zoom_start_sample;
                        if (zoom_data.size() > 1) {
                            x = plot_min.x + point_in_zoom * x_step;
                        } else {
                            x = plot_min.x + width * 0.5f;
                        }
                    } else {
                        if (m_pcm_data.size() > 1) {
                            x = plot_min.x + (*point) * x_step;
                        } else {
                            x = plot_min.x + width * 0.5f;
                        }
                    }
                    
                    if (x > plot_max.x) x = plot_max.x;
                    if (x < plot_min.x) x = plot_min.x;

                    ImVec2 tab_pos = is_top ? ImVec2(x, plot_min.y) : ImVec2(x, plot_max.y);
                    
                    ImVec2 p1 = tab_pos;
                    ImVec2 p2 = ImVec2(tab_pos.x - handle_size/2, tab_pos.y + (is_top ? -handle_size : handle_size));
                    ImVec2 p3 = ImVec2(tab_pos.x + handle_size/2, tab_pos.y + (is_top ? -handle_size : handle_size));
                    
                    ImGui::SetCursorScreenPos(ImVec2(p2.x, is_top ? p2.y : p1.y));
                    ImGui::InvisibleButton(id, ImVec2(handle_size, handle_size));
                    
                    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0))
                    {
                        float delta_x = ImGui::GetIO().MouseDelta.x;
                        int delta_samples = (int)(delta_x / x_step);
                        if (delta_samples != 0) {
                            *point += delta_samples;
                            if (*point < 0) *point = 0;
                            if (*point > (int)m_pcm_data.size()) *point = (int)m_pcm_data.size();
                            selection_changed = true;
                        }
                    }
                    
                    draw_list->AddTriangleFilled(p1, p2, p3, color);
                    draw_list->AddLine(ImVec2(x, plot_min.y), ImVec2(x, plot_max.y), color, 2.0f);
                };

                // Use unique IDs for tabs based on window instance
                std::string start_tab_id = "##start_tab_" + std::to_string(m_id);
                std::string end_tab_id = "##end_tab_" + std::to_string(m_id);
                
                handle_tab(&m_start_point, true, IM_COL32(0, 255, 0, 255), start_tab_id.c_str());
                handle_tab(&m_end_point, false, IM_COL32(255, 0, 0, 255), end_tab_id.c_str());
                
                bool is_playing = (m_preview_stream && !m_preview_stream->get_finished());
                if (is_playing && m_current_playback_position >= 0) {
                    float x;
                    if (m_zoom_enabled && !zoom_data.empty()) {
                        int pos_in_zoom = m_current_playback_position - zoom_start_sample;
                        if (zoom_data.size() > 1 && pos_in_zoom >= 0 && pos_in_zoom < (int)zoom_data.size()) {
                            x = plot_min.x + pos_in_zoom * x_step;
                        } else {
                            x = -1;
                        }
                    } else {
                        if (m_pcm_data.size() > 1) {
                            x = plot_min.x + m_current_playback_position * x_step;
                        } else {
                            x = plot_min.x + width * 0.5f;
                        }
                    }
                    
                    if (x >= 0) {
                        if (x > plot_max.x) x = plot_max.x;
                        if (x < plot_min.x) x = plot_min.x;
                        draw_list->AddLine(ImVec2(x, plot_min.y), ImVec2(x, plot_max.y), IM_COL32(0, 150, 255, 255), 2.0f);
                    }
                }
            }

            // Preview controls directly under the waveform
            bool is_playing = (m_preview_stream && !m_preview_stream->get_finished());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + margin_y * 0.5f);
            ImGui::Checkbox("Loop Preview", &m_preview_loop);
            ImGui::SameLine();
            const char* preview_label = is_playing ? "Stop Preview" : "Preview     ";
            ImVec2 preview_size = ImVec2(ImGui::CalcTextSize("Stop Preview").x + ImGui::GetStyle().FramePadding.x * 2.0f, 0);
            if (ImGui::Button(preview_label, preview_size))
            {
                if (is_playing)
                    StopPreview();
                else
                    StartPreview();
            }
            // Place the slice edit button on the same row, aligned right
            ImGui::SameLine();
            float right_button_width = 170.0f;
            float cursor_x = ImGui::GetWindowContentRegionMin().x + ImGui::GetContentRegionAvail().x - right_button_width;
            if (cursor_x < ImGui::GetCursorPosX()) cursor_x = ImGui::GetCursorPosX();
            ImGui::SetCursorPosX(cursor_x);
            if (ImGui::Button("Edit Slice In New Window", ImVec2(right_button_width, 0))) {
                StopPreview();
                ExportToNewWindow();
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + margin_y);
            
            int max_sample = (int)m_pcm_data.size();
            if (m_end_point > max_sample) m_end_point = max_sample;
            if (m_start_point >= m_end_point) m_start_point = m_end_point - 1;

            std::string start_point_id = "Start Point##" + std::to_string(m_id);
            std::string end_point_id = "End Point##" + std::to_string(m_id);
            if (ImGui::DragInt(start_point_id.c_str(), &m_start_point, 1.0f, 0, m_end_point - 1)) selection_changed = true;
            if (ImGui::DragInt(end_point_id.c_str(), &m_end_point, 1.0f, m_start_point + 1, max_sample)) selection_changed = true;

            if (selection_changed && is_playing) {
                StartPreview();
            }

            ImGui::Separator();
            ImGui::Checkbox("Double Speed", &m_double_speed);
            
            ImGui::Separator();
            ImGui::Checkbox("Enable Slicing", &m_slice_enabled);
            if (m_slice_enabled)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                ImGui::InputInt("Number of Slices", &m_num_slices, 1, 1);
                if (m_num_slices < 1) m_num_slices = 1;
                if (m_num_slices > 100) m_num_slices = 100;
            }
            
            ImGui::Separator();
            bool save_clicked = ImGui::Button("Export (17.5kHz Mono s16le)...");
            if (save_clicked)
            {
                StopPreview();
                m_browse_save = true;
                m_browse_open = false;
            }
            ImGui::SameLine();
            bool export_window_clicked = ImGui::Button("Export to New Window");
            if (export_window_clicked)
            {
                StopPreview();
                ExportToNewWindow();
            }

            static std::string pending_save_path;
            if (m_browse_save)
            {
                ImVec2 center = ImGui::GetIO().DisplaySize;
                center.x *= 0.5f;
                center.y *= 0.5f;
                ImVec2 size(600, 400);
                ImVec2 pos = ImVec2(center.x - size.x * 0.5f, center.y - size.y * 0.5f);

                const char* path = m_fs.saveFileDialog(save_clicked, m_input_path, m_slice_enabled ? "output.wav" : "output.wav", ".wav", m_slice_enabled ? "Save PCM (Base Name)" : "Save PCM", size, pos);
                if (strlen(path) > 0)
                {
                    if (m_slice_enabled)
                    {
                        ResampleAndSaveSlices(path);
                    }
                    else
                    {
                        ResampleAndSave(path);
                    }
                    m_browse_save = false;
                }
                else if (m_fs.hasUserJustCancelledDialog())
                {
                    m_browse_save = false;
                }
            }
        }
        
        ImGui::Separator();
        ImGui::TextWrapped("%s", m_status_message.c_str());
    }
    ImGui::End();
}

void PCMToolWindow::LoadFile(const char* filename)
{
    std::string filepath = filename;
    std::string temp_wav;
    bool is_mp3 = false;
    
    try {
        size_t dot_pos = filepath.find_last_of('.');
        if (dot_pos != std::string::npos) {
            std::string ext = filepath.substr(dot_pos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "mp3") {
                is_mp3 = true;
            }
        }
        
        if (is_mp3) {
            temp_wav = filepath + ".temp.wav";
            std::stringstream cmd;
            cmd << "ffmpeg -i \"" << filepath << "\" -f wav -acodec pcm_s16le -ar 44100 -ac 2 -y \"" << temp_wav << "\" 2>&1";
            
            int result = system(cmd.str().c_str());
            if (result != 0) {
                temp_wav = filepath + ".temp.wav";
                cmd.str("");
                cmd << "sox \"" << filepath << "\" -r 44100 -c 2 -b 16 \"" << temp_wav << "\" 2>&1";
                result = system(cmd.str().c_str());
                if (result != 0) {
                    m_status_message = "MP3 conversion failed. Please install ffmpeg or sox.";
                    return;
                }
            }
            filepath = temp_wav;
        }
        
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            m_status_message = "Failed to open audio file";
            if (is_mp3 && !temp_wav.empty()) {
                remove(temp_wav.c_str());
            }
            return;
        }

        char riff[4];
        file.read(riff, 4);
        if (memcmp(riff, "RIFF", 4) != 0) {
            m_status_message = "Not a valid WAV file (RIFF header missing)";
            if (is_mp3 && !temp_wav.empty()) {
                remove(temp_wav.c_str());
            }
            return;
        }

        uint32_t file_size;
        file.read((char*)&file_size, 4);

        char wave[4];
        file.read(wave, 4);
        if (memcmp(wave, "WAVE", 4) != 0) {
            m_status_message = "Not a valid WAV file (WAVE header missing)";
            if (is_mp3 && !temp_wav.empty()) {
                remove(temp_wav.c_str());
            }
            return;
        }

        uint16_t num_channels = 0;
        uint32_t sample_rate_val = 0;
        uint16_t bits_per_sample = 0;
        uint16_t audio_format = 0;
        std::vector<std::vector<int16_t>> channel_data;
        bool found_fmt = false;
        bool found_data = false;

        while (file && !found_data) {
            char chunk_id[4];
            uint32_t chunk_size;
            file.read(chunk_id, 4);
            if (file.gcount() != 4) break;
            file.read((char*)&chunk_size, 4);
            if (file.gcount() != 4) break;

            if (memcmp(chunk_id, "fmt ", 4) == 0) {
                file.read((char*)&audio_format, 2);
                file.read((char*)&num_channels, 2);
                file.read((char*)&sample_rate_val, 4);
                uint32_t byte_rate;
                file.read((char*)&byte_rate, 4);
                uint16_t block_align;
                file.read((char*)&block_align, 2);
                file.read((char*)&bits_per_sample, 2);
                
                if (chunk_size > 16) {
                    file.seekg(chunk_size - 16, std::ios::cur);
                }
                found_fmt = true;
            }
            else if (memcmp(chunk_id, "data", 4) == 0) {
                if (!found_fmt || num_channels == 0) {
                    m_status_message = "Invalid WAV format (missing format info)";
                    if (is_mp3 && !temp_wav.empty()) {
                        remove(temp_wav.c_str());
                    }
                    return;
                }

                if (audio_format != 1) {
                    m_status_message = "Unsupported audio format (only PCM supported)";
                    if (is_mp3 && !temp_wav.empty()) {
                        remove(temp_wav.c_str());
                    }
                    return;
                }

                channel_data.resize(num_channels);
                size_t bytes_per_sample = bits_per_sample / 8;
                size_t samples = chunk_size / (num_channels * bytes_per_sample);
                
                for (size_t i = 0; i < samples; ++i) {
                    for (int ch = 0; ch < num_channels; ++ch) {
                        int32_t sample_value = 0;
                        
                        if (bits_per_sample == 8) {
                            uint8_t sample_u8;
                            file.read((char*)&sample_u8, 1);
                            sample_value = ((int32_t)sample_u8 - 128) << 8;
                        }
                        else if (bits_per_sample == 16) {
                            int16_t sample_s16;
                            file.read((char*)&sample_s16, 2);
                            sample_value = (int32_t)sample_s16;
                        }
                        else if (bits_per_sample == 24) {
                            uint8_t bytes[3];
                            file.read((char*)bytes, 3);
                            int32_t sample_24 = (int32_t)(bytes[0] | (bytes[1] << 8) | (bytes[2] << 16));
                            if (sample_24 & 0x800000) sample_24 |= 0xFF000000;
                            sample_value = sample_24 >> 8;
                        }
                        else if (bits_per_sample == 32) {
                            int32_t sample_s32;
                            file.read((char*)&sample_s32, 4);
                            sample_value = sample_s32 >> 16;
                        }
                        else {
                            m_status_message = "Unsupported bit depth: " + std::to_string(bits_per_sample);
                            if (is_mp3 && !temp_wav.empty()) {
                                remove(temp_wav.c_str());
                            }
                            return;
                        }
                        
                        if (sample_value > 32767) sample_value = 32767;
                        if (sample_value < -32768) sample_value = -32768;
                        
                        channel_data[ch].push_back((int16_t)sample_value);
                    }
                }
                found_data = true;
            }
            else {
                if (chunk_size % 2 == 1) chunk_size++;
                file.seekg(chunk_size, std::ios::cur);
            }
        }

        if (!found_data || channel_data.empty() || channel_data[0].empty()) {
            m_status_message = "No audio data found in WAV file";
            if (is_mp3 && !temp_wav.empty()) {
                remove(temp_wav.c_str());
            }
            return;
        }

        m_sample_rate = sample_rate_val;
        m_channels = num_channels;
        size_t samples = channel_data[0].size();

        m_pcm_data.resize(samples);
        for(size_t i = 0; i < samples; ++i) {
            int32_t sum = 0;
            for(int c = 0; c < m_channels; ++c) {
                if (i < channel_data[c].size())
                    sum += channel_data[c][i];
            }
            m_pcm_data[i] = (short)(sum / m_channels);
        }

        m_start_point = 0;
        m_end_point = (int)samples;
        
        StopPreview();

        m_status_message = "Loaded " + std::string(filename);
        m_current_filename = filename;
        strncpy(m_input_path, filename, sizeof(m_input_path)-1);
        
        if (is_mp3 && !temp_wav.empty()) {
            remove(temp_wav.c_str());
        }
        
    } catch (std::exception& e) {
        m_status_message = "Error loading file: " + std::string(e.what());
        if (is_mp3 && !temp_wav.empty()) {
            remove(temp_wav.c_str());
        }
    }
}

void PCMToolWindow::SaveFile(const char* filename)
{
    // Not used directly
}

void PCMToolWindow::StartPreview()
{
    StopPreview();
    
    if (m_pcm_data.empty()) return;
    
    m_current_playback_position = m_start_point;
    
    std::shared_ptr<PCM_Preview_Stream> stream = std::make_shared<PCM_Preview_Stream>(
        m_pcm_data, m_start_point, m_end_point, m_sample_rate, m_preview_loop, &m_current_playback_position
    );
    
    m_preview_stream = stream;
    Audio_Manager::get().add_stream(m_preview_stream);
}

void PCMToolWindow::StopPreview()
{
    if (m_preview_stream)
    {
        m_preview_stream->set_finished(true);
        m_preview_stream.reset();
    }
    m_current_playback_position = -1;
}

void PCMToolWindow::ResampleAndSave(const char* filename)
{
    if (m_pcm_data.empty()) return;

    int target_rate = 17500;
    
    if (m_start_point < 0) m_start_point = 0;
    if (m_end_point > (int)m_pcm_data.size()) m_end_point = (int)m_pcm_data.size();
    if (m_start_point >= m_end_point) {
        m_status_message = "Invalid selection range";
        return;
    }

    std::vector<short> selection;
    selection.reserve(m_end_point - m_start_point);
    for(int i = m_start_point; i < m_end_point; ++i) {
        selection.push_back(m_pcm_data[i]);
    }

    std::vector<short> resampled;
    if (m_sample_rate == target_rate) {
        resampled = selection;
    } else {
        double ratio = (double)m_sample_rate / (double)target_rate;
        int new_length = (int)(selection.size() / ratio);
        resampled.resize(new_length);
        
        for(int i = 0; i < new_length; ++i) {
            double src_idx = i * ratio;
            int idx0 = (int)src_idx;
            int idx1 = idx0 + 1;
            float frac = (float)(src_idx - idx0);
            
            if (idx1 >= (int)selection.size()) idx1 = idx0;
            
            float s0 = selection[idx0];
            float s1 = selection[idx1];
            resampled[i] = (short)(s0 + (s1 - s0) * frac);
        }
    }

    // Apply speed doubling if enabled (skip every other sample)
    if (m_double_speed) {
        std::vector<short> speed_doubled;
        speed_doubled.reserve(resampled.size() / 2);
        for (size_t i = 0; i < resampled.size(); i += 2) {
            speed_doubled.push_back(resampled[i]);
        }
        resampled = speed_doubled;
    }

    std::ofstream out(filename, std::ios::binary);
    if (out) {
        uint32_t dataSize = (uint32_t)resampled.size() * sizeof(short);
        uint32_t fileSize = dataSize + 36;
        
        out.write("RIFF", 4);
        out.write((const char*)&fileSize, 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1;
        uint16_t numChannels = 1;
        uint32_t sampleRate = 17500;
        uint32_t byteRate = sampleRate * numChannels * sizeof(short);
        uint16_t blockAlign = numChannels * sizeof(short);
        uint16_t bitsPerSample = 16;
        
        out.write((const char*)&fmtSize, 4);
        out.write((const char*)&audioFormat, 2);
        out.write((const char*)&numChannels, 2);
        out.write((const char*)&sampleRate, 4);
        out.write((const char*)&byteRate, 4);
        out.write((const char*)&blockAlign, 2);
        out.write((const char*)&bitsPerSample, 2);
        
        out.write("data", 4);
        out.write((const char*)&dataSize, 4);
        
        out.write((char*)resampled.data(), dataSize);
        m_status_message = "Exported " + std::to_string(resampled.size()) + " samples to " + std::string(filename);
    } else {
        m_status_message = "Failed to write output file";
    }
}

void PCMToolWindow::ResampleAndSaveSlices(const char* base_filename)
{
    if (m_pcm_data.empty()) return;
    if (m_num_slices < 1) {
        m_status_message = "Invalid number of slices";
        return;
    }

    int target_rate = 17500;
    
    if (m_start_point < 0) m_start_point = 0;
    if (m_end_point > (int)m_pcm_data.size()) m_end_point = (int)m_pcm_data.size();
    if (m_start_point >= m_end_point) {
        m_status_message = "Invalid selection range";
        return;
    }

    std::vector<short> selection;
    selection.reserve(m_end_point - m_start_point);
    for(int i = m_start_point; i < m_end_point; ++i) {
        selection.push_back(m_pcm_data[i]);
    }

    std::vector<short> resampled;
    if (m_sample_rate == target_rate) {
        resampled = selection;
    } else {
        double ratio = (double)m_sample_rate / (double)target_rate;
        int new_length = (int)(selection.size() / ratio);
        resampled.resize(new_length);
        
        for(int i = 0; i < new_length; ++i) {
            double src_idx = i * ratio;
            int idx0 = (int)src_idx;
            int idx1 = idx0 + 1;
            float frac = (float)(src_idx - idx0);
            
            if (idx1 >= (int)selection.size()) idx1 = idx0;
            
            float s0 = selection[idx0];
            float s1 = selection[idx1];
            resampled[i] = (short)(s0 + (s1 - s0) * frac);
        }
    }

    // Apply speed doubling if enabled
    if (m_double_speed) {
        std::vector<short> speed_doubled;
        speed_doubled.reserve(resampled.size() / 2);
        for (size_t i = 0; i < resampled.size(); i += 2) {
            speed_doubled.push_back(resampled[i]);
        }
        resampled = speed_doubled;
    }

    std::string base_path = base_filename;
    if (base_path.length() >= 4 && base_path.substr(base_path.length() - 4) == ".wav")
    {
        base_path = base_path.substr(0, base_path.length() - 4);
    }
    
    int samples_per_slice = (int)resampled.size() / m_num_slices;
    int saved_count = 0;
    
    for (int slice = 0; slice < m_num_slices; ++slice)
    {
        int slice_start = slice * samples_per_slice;
        int slice_end = (slice == m_num_slices - 1) ? (int)resampled.size() : (slice + 1) * samples_per_slice;
        
        std::vector<short> slice_data;
        slice_data.reserve(slice_end - slice_start);
        for (int i = slice_start; i < slice_end; ++i)
        {
            slice_data.push_back(resampled[i]);
        }
        
        std::string slice_filename = base_path + "-" + std::to_string(slice + 1) + ".wav";
        
        std::ofstream out(slice_filename, std::ios::binary);
        if (out) {
            uint32_t dataSize = (uint32_t)slice_data.size() * sizeof(short);
            uint32_t fileSize = dataSize + 36;
            
            out.write("RIFF", 4);
            out.write((const char*)&fileSize, 4);
            out.write("WAVE", 4);
            out.write("fmt ", 4);
            
            uint32_t fmtSize = 16;
            uint16_t audioFormat = 1;
            uint16_t numChannels = 1;
            uint32_t sampleRate = 17500;
            uint32_t byteRate = sampleRate * numChannels * sizeof(short);
            uint16_t blockAlign = numChannels * sizeof(short);
            uint16_t bitsPerSample = 16;
            
            out.write((const char*)&fmtSize, 4);
            out.write((const char*)&audioFormat, 2);
            out.write((const char*)&numChannels, 2);
            out.write((const char*)&sampleRate, 4);
            out.write((const char*)&byteRate, 4);
            out.write((const char*)&blockAlign, 2);
            out.write((const char*)&bitsPerSample, 2);
            
            out.write("data", 4);
            out.write((const char*)&dataSize, 4);
            
            out.write((char*)slice_data.data(), dataSize);
            saved_count++;
        }
    }
    
    if (saved_count == m_num_slices) {
        m_status_message = "Exported " + std::to_string(m_num_slices) + " slices to " + base_path + "-*.wav";
    } else {
        m_status_message = "Exported " + std::to_string(saved_count) + " of " + std::to_string(m_num_slices) + " slices";
    }
}

void PCMToolWindow::LoadPCMData(const std::vector<short>& data, int rate, int ch, const std::string& name)
{
    m_pcm_data = data;
    m_sample_rate = rate;
    m_channels = ch;
    m_start_point = 0;
    m_end_point = (int)data.size();
    m_current_filename = name.empty() ? "Exported Selection" : name;
    m_status_message = "Loaded " + m_current_filename;
    StopPreview();
}

void PCMToolWindow::SetCreateWindowCallback(CreateWindowCallback callback)
{
    s_create_window_callback = callback;
}

void PCMToolWindow::ExportToNewWindow()
{
    if (m_pcm_data.empty()) {
        m_status_message = "No data to export";
        return;
    }

    if (!s_create_window_callback) {
        m_status_message = "Cannot create new window: no callback registered";
        return;
    }

    int target_rate = 17500;
    
    // 1. Extract selection
    if (m_start_point < 0) m_start_point = 0;
    if (m_end_point > (int)m_pcm_data.size()) m_end_point = (int)m_pcm_data.size();
    if (m_start_point >= m_end_point) {
        m_status_message = "Invalid selection range";
        return;
    }

    std::vector<short> selection;
    selection.reserve(m_end_point - m_start_point);
    for(int i = m_start_point; i < m_end_point; ++i) {
        selection.push_back(m_pcm_data[i]);
    }

    // 2. Resample
    std::vector<short> resampled;
    if (m_sample_rate == target_rate) {
        resampled = selection;
    } else {
        double ratio = (double)m_sample_rate / (double)target_rate;
        int new_length = (int)(selection.size() / ratio);
        resampled.resize(new_length);
        
        for(int i = 0; i < new_length; ++i) {
            double src_idx = i * ratio;
            int idx0 = (int)src_idx;
            int idx1 = idx0 + 1;
            float frac = (float)(src_idx - idx0);
            
            if (idx1 >= (int)selection.size()) idx1 = idx0;
            
            float s0 = selection[idx0];
            float s1 = selection[idx1];
            resampled[i] = (short)(s0 + (s1 - s0) * frac);
        }
    }

    // 3. Apply speed doubling if enabled
    if (m_double_speed) {
        std::vector<short> speed_doubled;
        speed_doubled.reserve(resampled.size() / 2);
        for (size_t i = 0; i < resampled.size(); i += 2) {
            speed_doubled.push_back(resampled[i]);
        }
        resampled = speed_doubled;
    }

    // 4. Create new window with processed data
    std::string export_name = m_current_filename.empty() ? "Exported Selection" : m_current_filename + " (exported)";
    
    auto new_window = std::make_shared<PCMToolWindow>();
    new_window->LoadPCMData(resampled, target_rate, 1, export_name);
    new_window->SetOpen(true);
    
    s_create_window_callback(new_window);
    
    m_status_message = "Exported " + std::to_string(resampled.size()) + " samples to new window";
}
