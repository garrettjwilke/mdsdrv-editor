## MDSDRV - Editor

A basic text editor built with ImGui, featuring a modular architecture.

### Project Structure

- `main.cpp` - Minimal entry point
- `window.h/cpp` - Window management and GLFW/ImGui initialization
- `editor.h/cpp` - Text editor logic and UI
- `theme.h/cpp` - Theme management (Dark, Light, Classic)

### Building

#### Prerequisites

- CMake 3.15 or higher
- GLFW3
- OpenGL 3.3+
- ImGui (will be downloaded or placed in `deps/imgui`)

#### Build Instructions

1. Clone ImGui into `deps/imgui`:
   ```bash
   mkdir -p deps
   cd deps
   git clone https://github.com/ocornut/imgui.git
   ```

2. Create build directory:
   ```bash
   mkdir build
   cd build
   ```

3. Configure and build:
   ```bash
   cmake ..
   make
   ```

4. Run:
   ```bash
   ./mdsdrv-editor
   ```

### Features

- Basic text editing with multiline support
- File menu (New, Open, Save, Save As)
- Edit menu (Cut, Copy, Paste, Undo, Redo)
- Status bar showing current file and unsaved changes indicator
- Dark theme by default
- Modular code structure

