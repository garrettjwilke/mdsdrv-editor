#define GL_SILENCE_DEPRECATION
#include "window.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include "config.h"

Window::Window() : m_window(nullptr), m_width(0), m_height(0) {
}

Window::~Window() {
    Shutdown();
}

bool Window::Initialize(int width, int height, const std::string& title) {
    m_width = width;
    m_height = height;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif

    // Create window
    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (!self) return;
        self->m_width = w;
        self->m_height = h;

        // Persist the latest window dimensions while preserving other config fields
        UserConfig cfg = LoadUserConfig();
        cfg.windowWidth = w;
        cfg.windowHeight = h;
        SaveUserConfig(cfg);
    });

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Place imgui.ini in user config dir instead of current working dir
    try {
        const char* home = std::getenv("HOME");
        std::filesystem::path cfgDir = home ? std::filesystem::path(home) / ".config" / "mdsdrv-editor"
                                            : std::filesystem::temp_directory_path() / "mdsdrv-editor";
        std::filesystem::create_directories(cfgDir);
        static std::string iniPath; // static to keep storage alive for ImGui
        iniPath = (cfgDir / "imgui.ini").string();
        io.IniFilename = iniPath.c_str();
    } catch (...) {
        // Fallback: disable saving if path resolution fails
        io.IniFilename = nullptr;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    
    // Use correct GLSL version based on platform
    #ifdef __EMSCRIPTEN__
        // WebGL2 uses GLSL ES 3.00
        ImGui_ImplOpenGL3_Init("#version 300 es");
    #else
        // Desktop OpenGL
        ImGui_ImplOpenGL3_Init("#version 330");
    #endif

    return true;
}

void Window::Shutdown() {
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

void Window::BeginFrame() {
    glfwPollEvents();

    // Start the ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::EndFrame() {
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
}

bool Window::ShouldClose() const {
    return m_window ? glfwWindowShouldClose(m_window) : true;
}

