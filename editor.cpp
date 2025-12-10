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
#include "pattern_editor.h"
#include "theme.h"
#include "config.h"
#include "core.h"
#include "song.h"
#include "track.h"
#include "track_info.h"
#include "player.h"
#include <unordered_set>
#include <map>
#include <memory>
#include <cstdlib>
#include <climits>

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
    m_patternEditor = std::make_unique<PatternEditor>();
    
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
    RenderPatternEditor();
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
            if (ImGui::MenuItem("Pattern Editor...")) {
                if (m_patternEditor) {
                    m_patternEditor->SetOpen(true);
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
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Adjust for menu bar
    float menuBarHeight = ImGui::GetFrameHeight();
    ImGui::SetWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight - 20));

    // Reserve space at the bottom for controls and padding
    const float buttonBarHeight = 32.0f;
    const float verticalPadding = 12.0f;
    const float horizontalPadding = 12.0f;

    // Text input sized to leave room for the bottom bar
    ImVec2 available = ImGui::GetContentRegionAvail();
    float textHeight = std::max(100.0f, available.y - buttonBarHeight - (verticalPadding * 2));
    ImVec2 textSize = ImVec2(-1.0f, textHeight);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | 
                                ImGuiInputTextFlags_NoHorizontalScroll;
    
    // Ensure buffer is large enough (1MB default, grow as needed)
    if (m_textBuffer.size() < m_text.size() + 1) {
        m_textBuffer.resize(std::max(m_text.size() + 1, size_t(1024 * 1024)));
    }
    std::copy(m_text.begin(), m_text.end(), m_textBuffer.begin());
    m_textBuffer[m_text.size()] = '\0';
    
    // Update highlights during playback
    if (m_isPlaying) {
        ShowTrackPositions();
    } else {
        m_highlights.clear();
    }
    
    if (ImGui::InputTextMultiline("##TextEditor", m_textBuffer.data(), m_textBuffer.size(), 
                                   textSize, flags)) {
        m_text = std::string(m_textBuffer.data());
        m_unsavedChanges = true;
    }
    
    // Render highlights right after the text input (so GetItemRectMin/Max work correctly)
    RenderHighlights();

    // Bottom control bar with padding to keep it prominent
    ImGui::Dummy(ImVec2(0.0f, verticalPadding));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + horizontalPadding);

    // Left side: compile status (if available)
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
        } else {
            ImGui::TextUnformatted("");
        }
    } else {
        ImGui::TextUnformatted("");
    }

    // Right-aligned control cluster: Debug, Play, Stop
    ImGui::SameLine();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float spacing = style.ItemSpacing.x;
    const float buttonWidth = 80.0f;
    const float buttonHeight = 26.0f;
    float debugWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize("Debug").x;
    float clusterWidth = debugWidth + spacing + buttonWidth + spacing + buttonWidth;

    float startX = ImGui::GetCursorPosX();
    float fullWidth = ImGui::GetContentRegionAvail().x;
    float targetX = startX + std::max(0.0f, fullWidth - clusterWidth - horizontalPadding);
    ImGui::SetCursorPosX(targetX);

    ImGui::Checkbox("Debug", &m_debug);
    ImGui::SameLine();
    if (ImGui::Button("Play", ImVec2(buttonWidth, buttonHeight))) {
        PlayMML();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Stop", ImVec2(buttonWidth, buttonHeight))) {
        StopMML();
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

void Editor::RenderPatternEditor() {
    if (m_patternEditor) {
        m_patternEditor->Render();
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
        m_highlights.clear(); // Clear highlights when stopping
        DebugLog("Playback stopped");
    } else if (m_songManager) {
        DebugLog("StopMML() called but not playing");
    } else {
        DebugLog("ERROR: StopMML() called but Song_Manager is null!");
    }
}

//! Get the length of a subroutine (helper function for macro highlighting)
unsigned int Editor::GetSubroutineLengthHelper(Song& song, unsigned int param, unsigned int max_recursion)
{
    try
    {
        Track& track = song.get_track(param);
        if(track.get_event_count())
        {
            auto event = track.get_event(track.get_event_count() - 1);
            uint32_t end_time;
            if(event.type == Event::JUMP && max_recursion != 0)
                end_time = event.play_time + GetSubroutineLengthHelper(song, event.param, max_recursion - 1);
            else if(event.type == Event::LOOP_END && max_recursion != 0)
            {
                // Simplified loop length calculation
                unsigned int loop_count = event.param - 1;
                unsigned int loop_start_time = 0;
                int depth = 0;
                for(unsigned int pos = track.get_event_count() - 1; pos > 0; pos--)
                {
                    auto loop_event = track.get_event(pos);
                    if(loop_event.type == Event::LOOP_END)
                        depth++;
                    else if(loop_event.type == Event::LOOP_START)
                    {
                        if(depth)
                            depth--;
                        else
                        {
                            loop_start_time = loop_event.play_time;
                            break;
                        }
                    }
                }
                end_time = event.play_time + (event.play_time - loop_start_time) * loop_count;
            }
            else
                end_time = event.play_time + event.on_time + event.off_time;
            return end_time - track.get_event(0).play_time;
        }
    }
    catch(std::exception &e)
    {
    }
    return 0;
}

void Editor::ShowTrackPositions()
{
    m_highlights.clear();
    
    if (!m_songManager) {
        return;
    }
    
    Song_Manager::Track_Map map = {};
    unsigned int ticks = 0;

    auto tracks = m_songManager->get_tracks();
    if(tracks != nullptr)
        map = *tracks;

    auto player = m_songManager->get_player();
    if(player == nullptr || player->get_finished())
    {
        // Player not active, clear highlights
        m_highlights.clear();
        return;
    }
    
    ticks = player->get_driver()->get_player_ticks();

    auto song = m_songManager->get_song();
    if(song == nullptr)
    {
        m_highlights.clear();
        return;
    }

    for(auto track_it = map.begin(); track_it != map.end(); track_it++)
    {
        auto& info = track_it->second;
        int offset = 0;

        // calculate offset to first loop
        if(ticks > info.length && info.loop_length)
            offset = ((ticks - info.loop_start) / info.loop_length) * info.loop_length;

        // calculate position
        auto it = info.events.lower_bound(ticks - offset);
        if(it != info.events.begin())
        {
            --it;
            auto event = it->second;
            for(auto && i : event.references)
            {
                // Include all references for highlighting - empty filename means current file,
                // and we want to highlight macro tracks and rndpat patterns even if they have filenames
                m_highlights[i->get_line()].insert(i->get_column());
            }
        }

        // Also check if we're inside a JUMP event (macro call) by examining the actual track
        // This handles cases where the Track_Info doesn't have events during macro execution
        try
        {
            Track& track = song->get_track(track_it->first);
            unsigned int event_count = track.get_event_count();
            
            // Find JUMP events and PLATFORM events (rndpat) that might be active at the current tick position
            for(unsigned int pos = 0; pos < event_count; pos++)
            {
                auto track_event = track.get_event(pos);
                unsigned int local_ticks = ticks - offset;
                
                if(track_event.type == Event::JUMP)
                {
                    // Calculate if we're within this JUMP event's duration
                    unsigned int jump_start = track_event.play_time;
                    unsigned int jump_length = GetSubroutineLengthHelper(*song, track_event.param, 10);
                    unsigned int jump_end = jump_start + jump_length;
                    
                    if(local_ticks >= jump_start && local_ticks < jump_end)
                    {
                        // We're inside a macro call - get events from the macro track
                        unsigned int macro_offset = local_ticks - jump_start;
                        
                        // Check if the macro track is in our map
                        auto macro_it = map.find(track_event.param);
                        Track_Info* macro_info = nullptr;
                        std::unique_ptr<Track_Info> generated_macro_info;
                        
                        if(macro_it != map.end())
                        {
                            // Macro track is in the map (shouldn't happen for track_id >= 16, but check anyway)
                            macro_info = &macro_it->second;
                        }
                        else
                        {
                            // Macro track is not in map (track_id >= max_channels)
                            // Generate Track_Info for it on the fly
                            try
                            {
                                Track& macro_track = song->get_track(track_event.param);
                                generated_macro_info = std::make_unique<Track_Info>(Track_Info_Generator(*song, macro_track));
                                macro_info = generated_macro_info.get();
                            }
                            catch(std::exception&)
                            {
                                // Macro track doesn't exist, skip
                                continue;
                            }
                        }
                        
                        if(macro_info != nullptr)
                        {
                            int macro_offset_loop = 0;
                            
                            // Handle looping in macro track
                            if(macro_offset > macro_info->length && macro_info->loop_length)
                                macro_offset_loop = ((macro_offset - macro_info->loop_start) / macro_info->loop_length) * macro_info->loop_length;
                            
                            // Find events in the macro track
                            auto macro_event_it = macro_info->events.lower_bound(macro_offset - macro_offset_loop);
                            if(macro_event_it != macro_info->events.begin())
                            {
                                --macro_event_it;
                                auto macro_event = macro_event_it->second;
                                for(auto && ref : macro_event.references)
                                {
                                    m_highlights[ref->get_line()].insert(ref->get_column());
                                }
                            }
                        }
                    }
                }
                else if(track_event.type == Event::PLATFORM)
                {
                    // Check if this is an rndpat command
                    try
                    {
                        const Tag& tag = song->get_platform_command(track_event.param);
                        if(!tag.empty() && tag[0] == "rndpat")
                        {
                            // This is an rndpat command - check if we're within its duration
                            // rndpat randomly selects one of the specified macros
                            // track_event.play_time is relative to track start, need to adjust for loops
                            unsigned int rndpat_start_absolute = track_event.play_time;
                            
                            // Calculate the maximum possible length (longest macro)
                            unsigned int max_length = 0;
                            std::vector<uint16_t> possible_macros;
                            
                            // Parse macro track IDs from rndpat arguments (format: "*300", "*301", etc.)
                            for(size_t i = 1; i < tag.size(); i++)
                            {
                                const std::string& arg = tag[i];
                                if(!arg.empty() && arg[0] == '*')
                                {
                                    int macro_id = std::strtol(arg.c_str() + 1, nullptr, 10);
                                    possible_macros.push_back(macro_id);
                                    
                                    // Calculate length of this macro
                                    try
                                    {
                                        unsigned int macro_length = GetSubroutineLengthHelper(*song, macro_id, 10);
                                        if(macro_length > max_length)
                                            max_length = macro_length;
                                    }
                                    catch(std::exception&)
                                    {
                                        // Macro doesn't exist, skip
                                    }
                                }
                            }
                            
                            // Check if we're inside this rndpat call in the current loop iteration
                            // rndpat_start_absolute is the play_time from the first iteration
                            // local_ticks is already adjusted for loops (ticks - offset)
                            
                            // If rndpat is before the loop point, check normally
                            // If rndpat is after the loop point, it repeats in each loop iteration
                            bool in_rndpat = false;
                            unsigned int rndpat_offset = 0;
                            
                            if(info.loop_length > 0 && rndpat_start_absolute >= info.loop_start)
                            {
                                // rndpat is in the loop section - check if we're in it in the current loop iteration
                                // local_ticks is already adjusted for loops, so we need to check
                                // if we're in the loop section and at the rndpat position within the loop
                                if(local_ticks >= info.loop_start)
                                {
                                    // We're in the loop section
                                    unsigned int position_in_loop = local_ticks - info.loop_start;
                                    unsigned int rndpat_position_in_loop = rndpat_start_absolute - info.loop_start;
                                    
                                    if(position_in_loop >= rndpat_position_in_loop && 
                                       position_in_loop < rndpat_position_in_loop + max_length)
                                    {
                                        in_rndpat = true;
                                        rndpat_offset = position_in_loop - rndpat_position_in_loop;
                                    }
                                }
                            }
                            else
                            {
                                // rndpat is before the loop point - check if we're in the first iteration
                                // Only check if we haven't reached the loop point yet
                                if(local_ticks < info.loop_start && 
                                   local_ticks >= rndpat_start_absolute && 
                                   local_ticks < rndpat_start_absolute + max_length)
                                {
                                    in_rndpat = true;
                                    rndpat_offset = local_ticks - rndpat_start_absolute;
                                }
                            }
                            
                            if(in_rndpat)
                            {
                                // We're inside an rndpat call - check all possible macro tracks
                                // to see which one matches the current tick position
                                // Since rndpat randomly selects a macro, we need to check all
                                // possible macros and find the one whose events align with the current position
                                
                                uint16_t best_macro = 0;
                                unsigned int best_match_score = 0;
                                
                                for(uint16_t macro_id : possible_macros)
                                {
                                    try
                                    {
                                        Track& macro_track = song->get_track(macro_id);
                                        Track_Info macro_info = Track_Info_Generator(*song, macro_track);
                                        
                                        // Calculate offset into macro, accounting for loops
                                        int macro_offset_loop = 0;
                                        unsigned int adjusted_offset = rndpat_offset;
                                        
                                        if(adjusted_offset > macro_info.length && macro_info.loop_length)
                                            macro_offset_loop = ((adjusted_offset - macro_info.loop_start) / macro_info.loop_length) * macro_info.loop_length;
                                        
                                        adjusted_offset -= macro_offset_loop;
                                        
                                        // Check if this offset is within the macro's valid range
                                        if(adjusted_offset <= macro_info.length)
                                        {
                                            // Find the event at or before this position
                                            auto macro_event_it = macro_info.events.lower_bound(adjusted_offset);
                                            if(macro_event_it != macro_info.events.begin())
                                            {
                                                --macro_event_it;
                                                auto macro_event = macro_event_it->second;
                                                unsigned int event_start = macro_event_it->first;
                                                unsigned int event_end = event_start + macro_event.on_time + macro_event.off_time;
                                                
                                                // Calculate how well this macro matches
                                                // Score is higher if we're within an event's duration
                                                unsigned int match_score = 0;
                                                if(adjusted_offset >= event_start && adjusted_offset < event_end)
                                                {
                                                    // We're within an event - this is a strong match
                                                    match_score = event_end - adjusted_offset; // Closer to start = higher score
                                                }
                                                else if(adjusted_offset >= event_start)
                                                {
                                                    // We're past the event but close
                                                    match_score = 1;
                                                }
                                                
                                                if(match_score > best_match_score)
                                                {
                                                    best_match_score = match_score;
                                                    best_macro = macro_id;
                                                }
                                            }
                                        }
                                    }
                                    catch(std::exception&)
                                    {
                                        // Macro doesn't exist, skip
                                        continue;
                                    }
                                }
                                
                                // Highlight the best matching macro
                                if(best_macro != 0)
                                {
                                    try
                                    {
                                        Track& macro_track = song->get_track(best_macro);
                                        Track_Info macro_info = Track_Info_Generator(*song, macro_track);
                                        
                                        int macro_offset_loop = 0;
                                        unsigned int adjusted_offset = rndpat_offset;
                                        
                                        if(adjusted_offset > macro_info.length && macro_info.loop_length)
                                            macro_offset_loop = ((adjusted_offset - macro_info.loop_start) / macro_info.loop_length) * macro_info.loop_length;
                                        
                                        adjusted_offset -= macro_offset_loop;
                                        
                                        auto macro_event_it = macro_info.events.lower_bound(adjusted_offset);
                                        if(macro_event_it != macro_info.events.begin())
                                        {
                                            --macro_event_it;
                                            auto macro_event = macro_event_it->second;
                                            for(auto && ref : macro_event.references)
                                            {
                                                m_highlights[ref->get_line()].insert(ref->get_column());
                                            }
                                        }
                                    }
                                    catch(std::exception&)
                                    {
                                        // Ignore errors
                                    }
                                }
                            }
                        }
                    }
                    catch(std::exception&)
                    {
                        // Platform command doesn't exist or error parsing, skip
                        continue;
                    }
                }
            }
        }
        catch(std::exception&)
        {
            // Track might not exist, skip it
        }
    }
    
    // Also check all macro tracks (track_id >= 16) to see if we're currently playing one
    // This handles rndpat sequences - we check which macro track matches the current position
    // and if it's referenced by an rndpat command, we highlight it
    try
    {
        Track_Map& all_tracks = song->get_track_map();
        
        // First, collect all active rndpat commands and their possible macros
        std::map<uint16_t, std::vector<uint16_t>> active_rndpat_macros; // track_id -> list of possible macro IDs
        
        for(auto track_it = map.begin(); track_it != map.end(); track_it++)
        {
            try
            {
                Track& track = song->get_track(track_it->first);
                auto& info = track_it->second;
                int offset = 0;
                
                if(ticks > info.length && info.loop_length)
                    offset = ((ticks - info.loop_start) / info.loop_length) * info.loop_length;
                
                unsigned int local_ticks = ticks - offset;
                unsigned int event_count = track.get_event_count();
                
                for(unsigned int pos = 0; pos < event_count; pos++)
                {
                    auto track_event = track.get_event(pos);
                    if(track_event.type == Event::PLATFORM)
                    {
                        try
                        {
                            const Tag& tag = song->get_platform_command(track_event.param);
                            if(!tag.empty() && tag[0] == "rndpat")
                            {
                                unsigned int rndpat_start_absolute = track_event.play_time;
                                unsigned int max_length = 0;
                                std::vector<uint16_t> possible_macros;
                                
                                for(size_t i = 1; i < tag.size(); i++)
                                {
                                    const std::string& arg = tag[i];
                                    if(!arg.empty() && arg[0] == '*')
                                    {
                                        int macro_id = std::strtol(arg.c_str() + 1, nullptr, 10);
                                        possible_macros.push_back(macro_id);
                                        
                                        try
                                        {
                                            unsigned int macro_length = GetSubroutineLengthHelper(*song, macro_id, 10);
                                            if(macro_length > max_length)
                                                max_length = macro_length;
                                        }
                                        catch(std::exception&) {}
                                    }
                                }
                                
                                // Check if we're inside this rndpat call
                                bool in_rndpat = false;
                                if(info.loop_length > 0 && rndpat_start_absolute >= info.loop_start)
                                {
                                    if(local_ticks >= info.loop_start)
                                    {
                                        unsigned int position_in_loop = local_ticks - info.loop_start;
                                        unsigned int rndpat_position_in_loop = rndpat_start_absolute - info.loop_start;
                                        
                                        if(position_in_loop >= rndpat_position_in_loop && 
                                           position_in_loop < rndpat_position_in_loop + max_length)
                                        {
                                            in_rndpat = true;
                                        }
                                    }
                                }
                                else
                                {
                                    if(local_ticks < info.loop_start && 
                                       local_ticks >= rndpat_start_absolute && 
                                       local_ticks < rndpat_start_absolute + max_length)
                                    {
                                        in_rndpat = true;
                                    }
                                }
                                
                                if(in_rndpat)
                                {
                                    // Store the possible macros for this rndpat
                                    active_rndpat_macros[track_it->first] = possible_macros;
                                }
                            }
                        }
                        catch(std::exception&) {}
                    }
                }
            }
            catch(std::exception&) {}
        }
        
        // Now check all macro tracks to see which one matches the current position
        for(auto& track_pair : all_tracks)
        {
            uint16_t macro_track_id = track_pair.first;
            if(macro_track_id < 16)
                continue; // Only check macro tracks
            
            try
            {
                Track& macro_track = track_pair.second;
                Track_Info macro_info = Track_Info_Generator(*song, macro_track);
                
                // Check if this macro track has events that match the current tick position
                // We need to check if we're inside a call to this macro
                // Calculate the offset into the macro based on when it was called
                
                // For each track that might have called this macro (via rndpat or JUMP)
                for(auto track_it = map.begin(); track_it != map.end(); track_it++)
                {
                    auto& info = track_it->second;
                    int offset = 0;
                    
                    if(ticks > info.length && info.loop_length)
                        offset = ((ticks - info.loop_start) / info.loop_length) * info.loop_length;
                    
                    unsigned int local_ticks = ticks - offset;
                    
                    // Check if this macro is referenced by an active rndpat
                    auto rndpat_it = active_rndpat_macros.find(track_it->first);
                    if(rndpat_it != active_rndpat_macros.end())
                    {
                        // Check if this macro is one of the possible macros
                        bool is_possible_macro = false;
                        for(uint16_t possible_id : rndpat_it->second)
                        {
                            if(possible_id == macro_track_id)
                            {
                                is_possible_macro = true;
                                break;
                            }
                        }
                        
                        if(is_possible_macro)
                        {
                            // Calculate the offset into the rndpat call
                            try
                            {
                                Track& track = song->get_track(track_it->first);
                                unsigned int event_count = track.get_event_count();
                                
                                for(unsigned int pos = 0; pos < event_count; pos++)
                                {
                                    auto track_event = track.get_event(pos);
                                    if(track_event.type == Event::PLATFORM)
                                    {
                                        try
                                        {
                                            const Tag& tag = song->get_platform_command(track_event.param);
                                            if(!tag.empty() && tag[0] == "rndpat")
                                            {
                                                unsigned int rndpat_start_absolute = track_event.play_time;
                                                unsigned int rndpat_offset = 0;
                                                
                                                if(info.loop_length > 0 && rndpat_start_absolute >= info.loop_start)
                                                {
                                                    if(local_ticks >= info.loop_start)
                                                    {
                                                        unsigned int position_in_loop = local_ticks - info.loop_start;
                                                        unsigned int rndpat_position_in_loop = rndpat_start_absolute - info.loop_start;
                                                        
                                                        if(position_in_loop >= rndpat_position_in_loop)
                                                        {
                                                            rndpat_offset = position_in_loop - rndpat_position_in_loop;
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    if(local_ticks >= rndpat_start_absolute)
                                                    {
                                                        rndpat_offset = local_ticks - rndpat_start_absolute;
                                                    }
                                                }
                                                
                                                if(rndpat_offset > 0)
                                                {
                                                    // Check if this macro's events match the offset
                                                    int macro_offset_loop = 0;
                                                    unsigned int adjusted_offset = rndpat_offset;
                                                    
                                                    if(adjusted_offset > macro_info.length && macro_info.loop_length)
                                                        macro_offset_loop = ((adjusted_offset - macro_info.loop_start) / macro_info.loop_length) * macro_info.loop_length;
                                                    
                                                    adjusted_offset -= macro_offset_loop;
                                                    
                                                    if(adjusted_offset <= macro_info.length)
                                                    {
                                                        auto macro_event_it = macro_info.events.lower_bound(adjusted_offset);
                                                        if(macro_event_it != macro_info.events.begin())
                                                        {
                                                            --macro_event_it;
                                                            auto macro_event = macro_event_it->second;
                                                            unsigned int event_start = macro_event_it->first;
                                                            unsigned int event_end = event_start + macro_event.on_time + macro_event.off_time;
                                                            
                                                            if(adjusted_offset >= event_start && adjusted_offset < event_end)
                                                            {
                                                                // This macro matches! Highlight it
                                                                for(auto && ref : macro_event.references)
                                                                {
                                                                    m_highlights[ref->get_line()].insert(ref->get_column());
                                                                }
                                                                // Found a match, break out of loops
                                                                goto found_macro_match;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        catch(std::exception&) {}
                                    }
                                }
                            }
                            catch(std::exception&) {}
                        }
                    }
                }
            }
            catch(std::exception&) {}
        }
        found_macro_match:;
    }
    catch(std::exception&)
    {
        // Error, skip
    }
}

void Editor::RenderHighlights()
{
    if (m_highlights.empty()) {
        return;
    }
    
    // Get the text input widget's rectangle (must be called right after InputTextMultiline)
    ImVec2 frame_min = ImGui::GetItemRectMin();
    ImVec2 frame_max = ImGui::GetItemRectMax();
    ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
    
    // Get draw list
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Get theme-aware highlight color
    // Use colors that complement each theme and provide good visibility
    ImU32 highlight_color;
    switch (m_themeSelection) {
        case 0: // Dark theme - use bright, warm color
            highlight_color = IM_COL32(255, 200, 50, 140); // Bright yellow-orange, visible on dark
            break;
        case 1: // Light theme - use darker, more saturated color
            highlight_color = IM_COL32(255, 160, 0, 160); // Darker orange-yellow, visible on light
            break;
        case 2: // Classic theme - use classic yellow
            highlight_color = IM_COL32(255, 255, 0, 120); // Classic yellow
            break;
        default:
            // Fallback: use a theme-adaptive color based on ImGui's current style
            {
                // Use HeaderHovered color which adapts to theme, but make it more visible
                ImVec4 header_hovered = ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered];
                // Blend with a warm tone for better visibility
                highlight_color = IM_COL32(
                    (int)((header_hovered.x * 0.7f + 1.0f * 0.3f) * 255), // Add some red
                    (int)((header_hovered.y * 0.7f + 0.8f * 0.3f) * 255), // Add some green
                    (int)((header_hovered.z * 0.7f + 0.2f * 0.3f) * 255), // Add some blue
                    (int)(header_hovered.w * 255 * 0.9f)
                );
            }
            break;
    }
    
    // Get font size for line height calculation
    float line_height = ImGui::GetTextLineHeight();
    float char_width = ImGui::CalcTextSize("M").x; // Use 'M' as a typical character width
    
    // Calculate text start position (inside the frame, accounting for padding)
    ImVec2 text_start = ImVec2(frame_min.x + frame_padding.x, frame_min.y + frame_padding.y);
    
    // Split text into lines and render highlights
    std::istringstream text_stream(m_text);
    std::string line;
    int line_num = 0;
    
    while (std::getline(text_stream, line)) {
        auto highlight_it = m_highlights.find(line_num);
        if (highlight_it != m_highlights.end()) {
            // This line has highlights
            const auto& columns = highlight_it->second;
            
            for (int col : columns) {
                if (col >= 0 && col <= (int)line.length()) {
                    // Calculate position for this character
                    // For multiline text, we need to account for the actual character positions
                    std::string line_prefix = line.substr(0, col);
                    float x = text_start.x + ImGui::CalcTextSize(line_prefix.c_str()).x;
                    float y = text_start.y + line_num * line_height;
                    
                    // Draw highlight rectangle for the character
                    // If at end of line, highlight a small width
                    float width = (col < (int)line.length()) ? char_width : char_width * 0.5f;
                    ImVec2 min_pos(x, y);
                    ImVec2 max_pos(x + width, y + line_height);
                    
                    draw_list->AddRectFilled(min_pos, max_pos, highlight_color);
                }
            }
        }
        line_num++;
    }
}

