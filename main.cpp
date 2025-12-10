#include "window.h"
#include "editor.h"
#include "theme.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <imgui.h>
#include "deps/imgui/examples/libs/emscripten/emscripten_mainloop_stub.h"
#endif

int main() {
    Window window;
    if (!window.Initialize(1280, 720, "MDSDRV Editor")) {
        return -1;
    }

    Editor editor;
    Theme::ApplyDefault();

#ifdef __EMSCRIPTEN__
    // Disable file system access for Emscripten
    ImGui::GetIO().IniFilename = nullptr;
    
    // Emscripten main loop
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    // Native main loop
    while (!window.ShouldClose())
#endif
    {
        window.BeginFrame();
        
        editor.Render();
        
        window.EndFrame();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    window.Shutdown();
    return 0;
}

