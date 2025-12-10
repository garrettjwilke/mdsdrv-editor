#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <vector>
#include <memory>
#include <list>
#include "config.h"

// Forward declarations
class Song_Manager;
class ExportWindow;
class PCMToolWindow;
class MDSBinExportWindow;

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
    void StopMML();

private:
    std::string m_text;
    std::string m_filepath;
    bool m_unsavedChanges;
    std::vector<char> m_textBuffer;
    std::unique_ptr<Song_Manager> m_songManager;
    std::unique_ptr<ExportWindow> m_exportWindow;
    std::unique_ptr<PCMToolWindow> m_pcmToolWindow;
    std::unique_ptr<MDSBinExportWindow> m_mdsBinExportWindow;
    std::list<std::shared_ptr<PCMToolWindow>> m_pcmToolWindows;
    bool m_isPlaying;
    bool m_debug;
    bool m_showThemeWindow;
    bool m_themeRequestFocus;
    int m_themeSelection; // 0=Dark,1=Light,2=Classic
    
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
    void RenderExportWindow();
    void RenderMDSBinExportWindow();
    void RenderThemeWindow();
    void RenderPCMToolWindow();
    bool CheckUnsavedChanges();
    void UpdateBuffer();
    void PlayMML();
    void DebugLog(const std::string& message);
};

#endif // EDITOR_H

