#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <vector>

class Editor {
public:
    Editor();
    
    void Render();
    void OpenFile(const std::string& filepath);
    void SaveFile(const std::string& filepath);
    void NewFile();

private:
    std::string m_text;
    std::string m_filepath;
    bool m_unsavedChanges;
    std::vector<char> m_textBuffer;
    
    void RenderMenuBar();
    void RenderTextEditor();
    void RenderStatusBar();
    void UpdateBuffer();
};

#endif // EDITOR_H

