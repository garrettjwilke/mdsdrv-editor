#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class Song_Manager;

class Editor {
public:
    Editor();
    ~Editor();
    
    void Render();
    void OpenFile(const std::string& filepath);
    void SaveFile(const std::string& filepath);
    void NewFile();
    void SetDebug(bool enabled) { m_debug = enabled; }
    bool GetDebug() const { return m_debug; }

private:
    std::string m_text;
    std::string m_filepath;
    bool m_unsavedChanges;
    std::vector<char> m_textBuffer;
    std::unique_ptr<Song_Manager> m_songManager;
    bool m_isPlaying;
    bool m_debug;
    
    // File dialogs
    bool m_showOpenDialog;
    bool m_showSaveDialog;
    bool m_showSaveAsDialog;
    
    // Confirmation dialogs
    bool m_showConfirmNewDialog;
    bool m_showConfirmOpenDialog;
    bool m_pendingNewFile;
    bool m_pendingOpenFile;
    
    void RenderMenuBar();
    void RenderTextEditor();
    void RenderStatusBar();
    void RenderFileDialogs();
    void RenderConfirmDialogs();
    bool CheckUnsavedChanges();
    void UpdateBuffer();
    void PlayMML();
    void StopMML();
    void DebugLog(const std::string& message);
};

#endif // EDITOR_H

