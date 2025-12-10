#include "window.h"
#include "editor.h"
#include "theme.h"

int main() {
    Window window;
    if (!window.Initialize(1280, 720, "MDSDRV Editor")) {
        return -1;
    }

    Editor editor;
    Theme::ApplyDefault();

    // Main loop
    while (!window.ShouldClose()) {
        window.BeginFrame();
        
        editor.Render();
        
        window.EndFrame();
    }

    window.Shutdown();
    return 0;
}

