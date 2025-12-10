#include "editor.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include "song_manager.h"
#include "audio_manager.h"
#include "imguifilesystem.h"
#include "export_window.h"
#include "pcm_tool_window.h"
#include "mdsbin_export_window.h"
#include "theme.h"
#include "config.h"

Editor::Editor() : m_unsavedChanges(false), m_isPlaying(false), m_debug(false),
                   m_showOpenDialog(false), m_showSaveDialog(false), m_showSaveAsDialog(false),
                   m_showConfirmNewDialog(false), m_showConfirmOpenDialog(false),
                   m_pendingNewFile(false), m_pendingOpenFile(false),
                   m_showThemeWindow(false), m_themeRequestFocus(false), m_themeSelection(0),
                   m_uiScale(1.0f) {
    m_text = "@3 psg 15\n\nH @3 o3 l4 a b c d\n";
    m_songManager = std::make_unique<Song_Manager>();
    m_exportWindow = std::make_unique<ExportWindow>();
    m_pcmToolWindow = std::make_unique<PCMToolWindow>();
    m_mdsBinExportWindow = std::make_unique<MDSBinExportWindow>();
    
    // Apply initial theme (higher-contrast dark)
    Theme::ApplyLight();

    // Load user config (theme selection and window state)
    UserConfig userConfig = LoadUserConfig();
    m_themeSelection = userConfig.theme;
    m_uiScale = userConfig.uiScale;
    switch (m_themeSelection) {
        case 0: Theme::ApplyDark(); break;
        case 1: Theme::ApplyLight(); break;
        case 2: Theme::ApplyClassic(); break;
        default: Theme::ApplyDark(); break;
    }
    ImGui::GetIO().FontGlobalScale = m_uiScale;
    
    // Set callback for creating new PCM tool windows
    PCMToolWindow::SetCreateWindowCallback([this](std::shared_ptr<PCMToolWindow> window) {
        m_pcmToolWindows.push_back(window);
    });
    
    UpdateBuffer();
}

Editor::~Editor() {
    StopMML();
}

void Editor::Render() {
    RenderMenuBar();
    RenderTextEditor();
    RenderStatusBar();
    RenderFileDialogs();
    RenderConfirmDialogs();
    RenderExportWindow();
    RenderMDSBinExportWindow();
    RenderThemeWindow();
    RenderPCMToolWindow();
}

void Editor::RenderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                if (CheckUnsavedChanges()) {
                    m_showConfirmNewDialog = true;
                    m_pendingNewFile = true;
                } else {
                    NewFile();
                }
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                if (CheckUnsavedChanges()) {
                    m_showConfirmOpenDialog = true;
                    m_pendingOpenFile = true;
                } else {
                    m_showOpenDialog = true;
                }
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                if (!m_filepath.empty()) {
                    SaveFile(m_filepath);
                } else {
                    m_showSaveAsDialog = true;
                }
            }
            if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                m_showSaveAsDialog = true;
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
                m_showThemeWindow = true;
                m_themeRequestFocus = true;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem("mdslink export...")) {
                if (m_exportWindow) {
                    m_exportWindow->SetOpen(true);
                }
            }
            if (ImGui::MenuItem("mdsdrv.bin export...")) {
                if (m_mdsBinExportWindow) {
                    m_mdsBinExportWindow->SetOpen(true);
                }
            }
            if (ImGui::MenuItem("PCM Tool...")) {
                if (m_pcmToolWindow) {
                    m_pcmToolWindow->SetOpen(true);
                }

                // Reopen and focus all existing PCM tool windows (including exported ones)
                for (auto& window : m_pcmToolWindows) {
                    if (window) {
                        window->SetOpen(true);
                    }
                }
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
    
    // Adjust for menu bar
    float menuBarHeight = ImGui::GetFrameHeight();
    ImGui::SetWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight - 20));
    
    // Play and Stop buttons at the top of the editor area
    if (ImGui::Button("Play", ImVec2(70, 20))) {
        PlayMML();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Stop", ImVec2(70, 20))) {
        StopMML();
    }
    
    ImGui::SameLine();
    
    // Show compile status
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
    
    ImGui::SameLine();
    ImGui::Checkbox("Debug", &m_debug);
    
    ImGui::Separator();
    
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

void Editor::RenderFileDialogs() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Open file dialog
    static ImGuiFs::Dialog openDialog;
    static bool openDialogWasOpen = false;
    
    // Create a trigger button state - true when menu item was just clicked
    bool openButtonPressed = m_showOpenDialog && !openDialogWasOpen;
    
    // Center the dialog on screen
    ImVec2 dialogSize(600, 400);
    ImVec2 dialogPos((io.DisplaySize.x - dialogSize.x) * 0.5f, (io.DisplaySize.y - dialogSize.y) * 0.5f);
    
    // Call the dialog - it manages its own state
    const char* openChosenPath = openDialog.chooseFileDialog(openButtonPressed, nullptr, ".mml;.txt;.*", "Open MML File", dialogSize, dialogPos);
    
    // Track if dialog is open
    openDialogWasOpen = m_showOpenDialog;
    
    // Check if a path was chosen
    if (strlen(openChosenPath) > 0) {
        OpenFile(openChosenPath);
        m_showOpenDialog = false;
        openDialogWasOpen = false;
        m_pendingOpenFile = false;
    } else if (m_showOpenDialog && strlen(openDialog.getChosenPath()) == 0 && !openButtonPressed) {
        // Dialog might have been closed - check if it's still trying to show
        // Reset if dialog is no longer active
        if (!openButtonPressed) {
            m_showOpenDialog = false;
            openDialogWasOpen = false;
            m_pendingOpenFile = false;
        }
    }
    
    // Save As dialog
    static ImGuiFs::Dialog saveAsDialog;
    static bool saveAsDialogWasOpen = false;
    
    // Create a trigger button state - true when menu item was just clicked
    bool saveButtonPressed = m_showSaveAsDialog && !saveAsDialogWasOpen;
    
    // Call the dialog - it manages its own state
    const char* defaultName = m_filepath.empty() ? "untitled.mml" : m_filepath.c_str();
    const char* saveChosenPath = saveAsDialog.saveFileDialog(saveButtonPressed, nullptr, defaultName, ".mml;.txt;.*", "Save MML File", dialogSize, dialogPos);
    
    // Track if dialog is open
    saveAsDialogWasOpen = m_showSaveAsDialog;
    
    // Check if a path was chosen
    if (strlen(saveChosenPath) > 0) {
        SaveFile(saveChosenPath);
        m_showSaveAsDialog = false;
        saveAsDialogWasOpen = false;
        
        // If we were pending a new file or open file, handle it now
        if (m_pendingNewFile) {
            NewFile();
            m_pendingNewFile = false;
        } else if (m_pendingOpenFile) {
            m_showOpenDialog = true;
            m_pendingOpenFile = false;
        }
    } else if (m_showSaveAsDialog && strlen(saveAsDialog.getChosenPath()) == 0 && !saveButtonPressed) {
        // Dialog might have been closed - reset
        m_showSaveAsDialog = false;
        saveAsDialogWasOpen = false;
        // Cancel pending actions if user cancelled save
        if (m_pendingNewFile || m_pendingOpenFile) {
            m_pendingNewFile = false;
            m_pendingOpenFile = false;
        }
    }
}

bool Editor::CheckUnsavedChanges() {
    return m_unsavedChanges;
}

void Editor::RenderConfirmDialogs() {
    // Confirm New File dialog
    if (m_showConfirmNewDialog) {
        ImGui::OpenPopup("Confirm New File");
        m_showConfirmNewDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Confirm New File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Do you want to save before creating a new file?");
        ImGui::Separator();
        
        if (ImGui::Button("Yes", ImVec2(100, 0))) {
            if (!m_filepath.empty()) {
                SaveFile(m_filepath);
                NewFile();
                m_pendingNewFile = false;
            } else {
                // Need to save as first
                m_showSaveAsDialog = true;
                m_pendingNewFile = true;
            }
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("No", ImVec2(100, 0))) {
            NewFile();
            m_pendingNewFile = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pendingNewFile = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Confirm Open File dialog
    if (m_showConfirmOpenDialog) {
        ImGui::OpenPopup("Confirm Open File");
        m_showConfirmOpenDialog = false;
    }
    
    if (ImGui::BeginPopupModal("Confirm Open File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("You have unsaved changes. Do you want to save before opening a new file?");
        ImGui::Separator();
        
        if (ImGui::Button("Yes", ImVec2(100, 0))) {
            if (!m_filepath.empty()) {
                SaveFile(m_filepath);
                m_showOpenDialog = true;
                m_pendingOpenFile = false;
            } else {
                // Need to save as first
                m_showSaveAsDialog = true;
                m_pendingOpenFile = true;
            }
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("No", ImVec2(100, 0))) {
            m_showOpenDialog = true;
            m_pendingOpenFile = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            m_pendingOpenFile = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void Editor::RenderExportWindow() {
    if (m_exportWindow) {
        m_exportWindow->Render();
    }
}

void Editor::RenderMDSBinExportWindow() {
    if (m_mdsBinExportWindow) {
        m_mdsBinExportWindow->Render();
    }
}

void Editor::RenderThemeWindow() {
    if (!m_showThemeWindow) return;

    ImGui::SetNextWindowSize(ImVec2(420, 260), ImGuiCond_FirstUseEver);
    if (m_themeRequestFocus) {
        ImGui::SetNextWindowFocus();
        m_themeRequestFocus = false;
    }

    if (ImGui::Begin("Theme", &m_showThemeWindow)) {
        ImGui::Text("Choose a theme:");
        ImGui::Separator();

        bool changed = false;
        changed |= ImGui::RadioButton("High-contrast Dark", &m_themeSelection, 0);
        changed |= ImGui::RadioButton("Light", &m_themeSelection, 1);
        changed |= ImGui::RadioButton("Classic", &m_themeSelection, 2);
        changed |= ImGui::SliderFloat("UI Scale", &m_uiScale, 0.5f, 2.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);

        if (changed) {
            switch (m_themeSelection) {
                case 0: Theme::ApplyDark(); break;
                case 1: Theme::ApplyLight(); break;
                case 2: Theme::ApplyClassic(); break;
                default: Theme::ApplyDark(); break;
            }
            ImGui::GetIO().FontGlobalScale = m_uiScale;
            // Persist the updated theme without losing window dimensions
            UserConfig cfg = LoadUserConfig();
            cfg.theme = m_themeSelection;
            cfg.uiScale = m_uiScale;
            SaveUserConfig(cfg);
        }

        ImGui::Separator();
        ImGui::TextWrapped("Tip: use Light mode if the dark palette is hard to read.");
    }
    ImGui::End();
}

void Editor::RenderPCMToolWindow() {
    // Render main PCM tool window
    if (m_pcmToolWindow) {
        m_pcmToolWindow->Render();
    }
    
    // Render all additional PCM tool windows
    // Remove closed windows from the list
    auto it = m_pcmToolWindows.begin();
    while (it != m_pcmToolWindows.end()) {
        if ((*it)->IsOpen()) {
            (*it)->Render();
            ++it;
        } else {
            it = m_pcmToolWindows.erase(it);
        }
    }
}

void Editor::DebugLog(const std::string& message) {
    if (!m_debug) return;
    std::cout << "[Editor DEBUG] " << message << std::endl;
}

void Editor::PlayMML() {
    if (!m_songManager) {
        DebugLog("ERROR: Song_Manager is null!");
        return;
    }
    
    DebugLog("PlayMML() called");
    DebugLog("MML text length: " + std::to_string(m_text.length()) + " characters");
    
    // Show first few lines of MML for debugging
    std::istringstream iss(m_text);
    std::string line;
    int lineCount = 0;
    while (std::getline(iss, line) && lineCount < 5) {
        DebugLog("MML line " + std::to_string(lineCount) + ": " + line);
        lineCount++;
    }
    
    // Stop any current playback
    StopMML();
    
    // Compile the MML text
    std::string filename = m_filepath.empty() ? "untitled.mml" : m_filepath;
    DebugLog("Starting compilation with filename: " + filename);
    int compileResult = m_songManager->compile(m_text, filename);
    
    if (compileResult != 0) {
        DebugLog("WARNING: compile() returned non-zero: " + std::to_string(compileResult));
    } else {
        DebugLog("Compilation started successfully");
    }
    
    // Wait for compilation to complete (with timeout)
    int timeout = 200; // Increased timeout
    int waitCount = 0;
    while (m_songManager->get_compile_in_progress() && timeout > 0) {
        --timeout;
        ++waitCount;
        // Small delay to allow compilation thread to work
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (waitCount > 0) {
        DebugLog("Waited " + std::to_string(waitCount) + " iterations for compilation");
    }
    
    // Check if compilation succeeded
    Song_Manager::Compile_Result result = m_songManager->get_compile_result();
    
    if (result == Song_Manager::COMPILE_OK) {
        DebugLog("Compilation successful! Starting playback...");
        
        // Check audio manager status
        Audio_Manager& am = Audio_Manager::get();
        
        // Check if we have a song
        auto song = m_songManager->get_song();
        
        try {
            m_songManager->play(0);
            m_isPlaying = true;
            DebugLog("Playback started successfully");
        } catch (const std::exception& e) {
            DebugLog("ERROR: Exception during play(): " + std::string(e.what()));
            m_isPlaying = false;
        } catch (...) {
            DebugLog("ERROR: Unknown exception during play()");
            m_isPlaying = false;
        }
    } else if (result == Song_Manager::COMPILE_ERROR) {
        std::string errorMsg = m_songManager->get_error_message();
        DebugLog("ERROR: Compilation failed: " + errorMsg);
    } else if (result == Song_Manager::COMPILE_NOT_DONE) {
        DebugLog("WARNING: Compilation still in progress or not started");
    }
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

