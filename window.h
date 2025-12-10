#ifndef WINDOW_H
#define WINDOW_H

#include <GLFW/glfw3.h>
#include <string>

class Window {
public:
    Window();
    ~Window();

    bool Initialize(int width, int height, const std::string& title);
    void Shutdown();
    
    void BeginFrame();
    void EndFrame();
    
    bool ShouldClose() const;
    GLFWwindow* GetGLFWWindow() const { return m_window; }

private:
    GLFWwindow* m_window;
    int m_width;
    int m_height;
};

#endif // WINDOW_H

