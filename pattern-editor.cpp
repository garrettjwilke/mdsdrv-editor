#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <vector>
#include <string>

// Include GLEW (or equivalent) and GLFW for a minimal runnable example
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
#include <GL/gl3w.h>
#endif
#if defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>
#endif
#if defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// --- Pattern Configuration ---
const int NOTE_COUNT = 12;      // C up to B (one octave)
const int PATTERN_STEPS = 16;
const char* NOTE_NAMES[NOTE_COUNT] = {
    "C ", "C#", "D ", "D#", "E ", "F ",
    "F#", "G ", "G#", "A ", "A#", "B "
};

struct AppState {
    // pattern_matrix[note_index][step_index]
    std::vector<std::vector<bool>> pattern_matrix;

    AppState() {
        // Initialize the 12x16 matrix with all notes off (false)
        pattern_matrix.resize(NOTE_COUNT);
        for (int i = 0; i < NOTE_COUNT; ++i) {
            pattern_matrix[i].resize(PATTERN_STEPS, false);
        }
    }
};

// Simple helper function to handle window creation/error checking
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Function to render the main pattern editor window
void RenderPatternEditorWindow(AppState& state) {
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (ImGui::Begin("CTRMML Pattern Sequencer", nullptr, ImGuiWindowFlags_HorizontalScrollbar)) {

        // Use a Child Window or equivalent to ensure the grid scrolls independently
        if (ImGui::BeginChild("PatternGrid", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {

            // --- 1. Step Number Headers (Horizontal Axis) ---
            ImGui::Text("Note \\ Step");
            float button_width = 30.0f;
            float button_height = 20.0f;

            for (int j = 0; j < PATTERN_STEPS; ++j) {
                ImGui::SameLine();
                // Highlight every 4th step (beat)
                if (j % 4 == 0) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f)); // Red text for beats
                }
                ImGui::Text("%02d", j + 1);
                if (j % 4 == 0) {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::Separator();

            // --- 2. Note Grid (Vertical Axis) ---
            for (int i = NOTE_COUNT - 1; i >= 0; --i) { // Iterate high notes down to low notes
                // Note Label (e.g., C, C#, D)
                ImGui::Text("%s", NOTE_NAMES[i]);

                // Draw the sequence steps (buttons) for this note
                for (int j = 0; j < PATTERN_STEPS; ++j) {
                    ImGui::SameLine();

                    // Style the button based on its state
                    if (state.pattern_matrix[i][j]) {
                        // Active state (Green/Yellow for ON)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));
                    } else if (j % 4 == 0) {
                        // Inactive state, strong beat (Darker grey/red)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.1f, 0.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.05f, 0.05f, 1.0f));
                    } else {
                        // Inactive state, regular step (Grey)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                    }

                    // Create a unique ID for the button (Note:Step)
                    std::string button_id = "##" + std::to_string(i) + "_" + std::to_string(j);

                    if (ImGui::Button(button_id.c_str(), ImVec2(button_width, button_height))) {
                        // Toggle the note state when clicked
                        state.pattern_matrix[i][j] = !state.pattern_matrix[i][j];
                    }

                    ImGui::PopStyleColor(3); // Pop the three style colors applied
                }
            }

            ImGui::EndChild();
        }


        // --- 3. Display Pattern Data (Debugging/Export) ---
        ImGui::Separator();
        ImGui::Text("Pattern Data (CTRMMML Style Pseudo-Export):");

        ImGui::TextWrapped("The sequence below shows the active notes per step. A '0' means C, '11' means B.");
        ImGui::TextWrapped("The pattern allows multiple notes per step (polyphony).");

        // Simple text output of the current pattern state
        std::string pattern_output = "[";
        for (int j = 0; j < PATTERN_STEPS; ++j) {
            pattern_output += " [";
            bool step_has_notes = false;
            for (int i = 0; i < NOTE_COUNT; ++i) {
                if (state.pattern_matrix[i][j]) {
                    if (step_has_notes) pattern_output += ", ";
                    pattern_output += std::to_string(i); // Output note index (0=C, 11=B)
                    step_has_notes = true;
                }
            }
            if (!step_has_notes) {
                 pattern_output += " - "; // Placeholder for silent step
            }
            pattern_output += " ]";
        }
        pattern_output += " ]";

        ImGui::InputTextMultiline("##PatternOutput", const_cast<char*>(pattern_output.c_str()), pattern_output.length(), ImVec2(0, 100), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::End();
}


// --- Boilerplate Setup/Main Loop ---

int main(int, char**)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "CTRMML Sequencer Demo", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0;
#else
    bool err = false; // If using another loader, set this to proper init check
#endif
    if (err)
    {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Our state
    AppState app_state;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 1. Show the main pattern editor window
        RenderPatternEditorWindow(app_state);

        // 2. Simple Main Menu/Demo Window (Optional)
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Exit")) { glfwSetWindowShouldClose(window, GLFW_TRUE); }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        
        ImGui::ShowDemoWindow(nullptr); // Show the ImGui demo window

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}