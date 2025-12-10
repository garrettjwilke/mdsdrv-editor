#include "editor.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include "song_manager.h"
#include "audio_manager.h"

Editor::Editor() : m_unsavedChanges(false), m_isPlaying(false), m_debug(false) {
    m_text = "// Welcome to MDSDRV Editor\n// Start typing...\n";
    m_songManager = std::make_unique<Song_Manager>();
    UpdateBuffer();
}

Editor::~Editor() {
    StopMML();
}

void Editor::Render() {
    RenderMenuBar();
    RenderPlaybackControls();
    RenderTextEditor();
    RenderStatusBar();
}

void Editor::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                NewFile();
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                // TODO: Implement file dialog
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_filepath.empty()) {
                    SaveFile(m_filepath);
                }
                // TODO: Implement save as dialog
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                // TODO: Implement file dialog
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // TODO: Handle exit
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "Ctrl+X")) {}
            if (ImGui::MenuItem("Copy", "Ctrl+C")) {}
            if (ImGui::MenuItem("Paste", "Ctrl+V")) {}
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Theme")) {
                // TODO: Theme selector
            }
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void Editor::RenderTextEditor() {
    ImGui::Begin("Text Editor", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | 
                 ImGuiWindowFlags_NoCollapse | 
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoResize);
    
    // Get available space
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    
    // Adjust for menu bar and playback controls
    float menuBarHeight = ImGui::GetFrameHeight();
    float controlsHeight = 30.0f;
    ImGui::SetWindowPos(ImVec2(0, menuBarHeight + controlsHeight));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight - controlsHeight - 20));
    
    // Text input
    ImVec2 textSize = ImVec2(-1.0f, -1.0f);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | 
                                ImGuiInputTextFlags_NoHorizontalScroll;
    
    // Ensure buffer is large enough (1MB default, grow as needed)
    if (m_textBuffer.size() < m_text.size() + 1) {
        m_textBuffer.resize(std::max(m_text.size() + 1, size_t(1024 * 1024)));
    }
    std::copy(m_text.begin(), m_text.end(), m_textBuffer.begin());
    m_textBuffer[m_text.size()] = '\0';
    
    if (ImGui::InputTextMultiline("##TextEditor", m_textBuffer.data(), m_textBuffer.size(), 
                                   textSize, flags)) {
        m_text = std::string(m_textBuffer.data());
        m_unsavedChanges = true;
    }
    
    ImGui::End();
}

void Editor::RenderStatusBar() {
    ImGui::Begin("Status", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | 
                 ImGuiWindowFlags_NoCollapse | 
                 ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoResize);
    
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetWindowPos(ImVec2(0, io.DisplaySize.y - 20));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, 20));
    
    std::string status = m_filepath.empty() ? "Untitled" : m_filepath;
    if (m_unsavedChanges) {
        status += " *";
    }
    
    ImGui::Text("%s", status.c_str());
    
    ImGui::End();
}

void Editor::OpenFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        m_text = buffer.str();
        m_filepath = filepath;
        m_unsavedChanges = false;
        file.close();
        UpdateBuffer();
    } else {
        std::cerr << "Failed to open file: " << filepath << std::endl;
    }
}

void Editor::SaveFile(const std::string& filepath) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << m_text;
        m_filepath = filepath;
        m_unsavedChanges = false;
        file.close();
    } else {
        std::cerr << "Failed to save file: " << filepath << std::endl;
    }
}

void Editor::NewFile() {
    if (m_unsavedChanges) {
        // TODO: Ask user if they want to save
    }
    m_text = "";
    m_filepath = "";
    m_unsavedChanges = false;
    UpdateBuffer();
}

void Editor::UpdateBuffer() {
    size_t requiredSize = std::max(m_text.size() + 1, size_t(1024));
    if (m_textBuffer.size() < requiredSize) {
        m_textBuffer.resize(requiredSize);
    }
    std::copy(m_text.begin(), m_text.end(), m_textBuffer.begin());
    m_textBuffer[m_text.size()] = '\0';
}

void Editor::RenderPlaybackControls() {
    ImGuiIO& io = ImGui::GetIO();
    float menuBarHeight = ImGui::GetFrameHeight();
    
    ImGui::Begin("Playback Controls", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | 
                 ImGuiWindowFlags_NoCollapse | 
                 ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoResize);
    
    ImGui::SetWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, 30));
    
    // Debug toggle
    ImGui::Checkbox("Debug", &m_debug);
    ImGui::SameLine();
    
    // Check compile status
    if (m_songManager) {
        Song_Manager::Compile_Result compileResult = m_songManager->get_compile_result();
        bool compileInProgress = m_songManager->get_compile_in_progress();
        
        if (compileInProgress) {
            ImGui::Text("Compiling...");
        } else if (compileResult == Song_Manager::COMPILE_ERROR) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Compile Error: %s", 
                              m_songManager->get_error_message().c_str());
        } else if (compileResult == Song_Manager::COMPILE_OK) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Compile OK");
        }
    }
    
    ImGui::SameLine(io.DisplaySize.x - 150);
    
    // Play button
    if (m_isPlaying) {
        if (ImGui::Button("Stop", ImVec2(70, 20))) {
            StopMML();
        }
    } else {
        if (ImGui::Button("Play", ImVec2(70, 20))) {
            PlayMML();
        }
    }
    
    ImGui::End();
}

void Editor::DebugLog(const std::string& message) {
    // Always log, but prefix with [DEBUG] if debug mode is on
    if (m_debug) {
        std::cout << "[Editor DEBUG] " << message << std::endl;
    } else {
        std::cout << "[Editor] " << message << std::endl;
    }
}

void Editor::PlayMML() {
    // Always output when Play is pressed
    std::cout << "[Editor] ====== PLAY BUTTON PRESSED ======" << std::endl;
    
    if (!m_songManager) {
        std::cout << "[Editor] ERROR: Song_Manager is null!" << std::endl;
        return;
    }
    
    DebugLog("PlayMML() called");
    DebugLog("MML text length: " + std::to_string(m_text.length()) + " characters");
    
    // Show first few lines of MML for debugging
    std::istringstream iss(m_text);
    std::string line;
    int lineCount = 0;
    std::cout << "[Editor] First 5 lines of MML:" << std::endl;
    while (std::getline(iss, line) && lineCount < 5) {
        std::cout << "[Editor]   " << lineCount << ": " << line << std::endl;
        lineCount++;
    }
    
    // Stop any current playback
    StopMML();
    
    // Compile the MML text
    std::string filename = m_filepath.empty() ? "untitled.mml" : m_filepath;
    DebugLog("Starting compilation with filename: " + filename);
    std::cout << "[Editor] Calling compile()..." << std::endl;
    
    int compileResult = m_songManager->compile(m_text, filename);
    std::cout << "[Editor] compile() returned: " << compileResult << std::endl;
    
    if (compileResult != 0) {
        DebugLog("WARNING: compile() returned non-zero: " + std::to_string(compileResult));
    } else {
        DebugLog("Compilation started successfully");
    }
    
    // Wait for compilation to complete (with timeout)
    int timeout = 200; // Increased timeout
    int waitCount = 0;
    std::cout << "[Editor] Waiting for compilation to complete..." << std::endl;
    
    while (m_songManager->get_compile_in_progress() && timeout > 0) {
        --timeout;
        ++waitCount;
        if (waitCount % 10 == 0) {
            std::cout << "[Editor] Still compiling... (" << waitCount << " iterations)" << std::endl;
        }
        // Small delay to allow compilation thread to work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (waitCount > 0) {
        DebugLog("Waited " + std::to_string(waitCount) + " iterations for compilation");
    }
    
    // Check if compilation succeeded
    Song_Manager::Compile_Result result = m_songManager->get_compile_result();
    std::cout << "[Editor] Compile result: " << result << " (0=OK, 1=ERROR, -1=NOT_DONE)" << std::endl;
    
    if (result == Song_Manager::COMPILE_OK) {
        DebugLog("Compilation successful! Starting playback...");
        
        // Check audio manager status
        Audio_Manager& am = Audio_Manager::get();
        std::cout << "[Editor] Audio status:" << std::endl;
        std::cout << "[Editor]   Enabled: " << (am.get_audio_enabled() ? "yes" : "no") << std::endl;
        std::cout << "[Editor]   Driver: " << am.get_driver() << std::endl;
        std::cout << "[Editor]   Device: " << am.get_device() << std::endl;
        std::cout << "[Editor]   Volume: " << am.get_volume() << std::endl;
        
        // Check if we have a song
        auto song = m_songManager->get_song();
        if (song) {
            std::cout << "[Editor] Song object is valid" << std::endl;
        } else {
            std::cout << "[Editor] WARNING: Song object is null!" << std::endl;
        }
        
        try {
            std::cout << "[Editor] Calling play(0)..." << std::endl;
            m_songManager->play(0);
            m_isPlaying = true;
            std::cout << "[Editor] Playback started successfully!" << std::endl;
            DebugLog("Playback started successfully");
        } catch (const std::exception& e) {
            std::cout << "[Editor] ERROR: Exception during play(): " << e.what() << std::endl;
            DebugLog("ERROR: Exception during play(): " + std::string(e.what()));
            m_isPlaying = false;
        } catch (...) {
            std::cout << "[Editor] ERROR: Unknown exception during play()" << std::endl;
            DebugLog("ERROR: Unknown exception during play()");
            m_isPlaying = false;
        }
    } else if (result == Song_Manager::COMPILE_ERROR) {
        std::string errorMsg = m_songManager->get_error_message();
        std::cout << "[Editor] ERROR: Compilation failed: " << errorMsg << std::endl;
        DebugLog("ERROR: Compilation failed: " + errorMsg);
    } else if (result == Song_Manager::COMPILE_NOT_DONE) {
        std::cout << "[Editor] WARNING: Compilation still in progress or not started" << std::endl;
        DebugLog("WARNING: Compilation still in progress or not started");
    }
    
    std::cout << "[Editor] ====== PLAY BUTTON HANDLING COMPLETE ======" << std::endl;
}

void Editor::StopMML() {
    if (m_songManager && m_isPlaying) {
        DebugLog("Stopping playback...");
        m_songManager->stop();
        m_isPlaying = false;
        DebugLog("Playback stopped");
    } else if (m_songManager) {
        DebugLog("StopMML() called but not playing");
    } else {
        DebugLog("ERROR: StopMML() called but Song_Manager is null!");
    }
}

