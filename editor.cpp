#include "editor.h"
#include <imgui.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

Editor::Editor() : m_unsavedChanges(false) {
    m_text = "// Welcome to MDSDRV Editor\n// Start typing...\n";
    UpdateBuffer();
}

void Editor::Render() {
    RenderMenuBar();
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
    
    // Adjust for menu bar
    float menuBarHeight = ImGui::GetFrameHeight();
    ImGui::SetWindowPos(ImVec2(0, menuBarHeight));
    ImGui::SetWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - menuBarHeight - 20));
    
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

