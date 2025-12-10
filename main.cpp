#include "window.h"
#include "editor.h"
#include "theme.h"
#include "config.h"
#include "deps/mmlgui/src/audio_manager.h"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <imgui.h>
#include "deps/imgui/examples/libs/emscripten/emscripten_mainloop_stub.h"
#endif

int main() {
    // Initialize Audio_Manager
    Audio_Manager& audioManager = Audio_Manager::get();
    audioManager.set_sample_rate(44100);
    std::cout << "[Main] Audio_Manager initialized with sample rate: 44100" << std::endl;
    
    // Set the audio driver - use the first available driver
    // On macOS this will be Core Audio, on Linux it will be PulseAudio or ALSA
    const auto& driverList = audioManager.get_driver_list();
    if (!driverList.empty()) {
        // Use the first available driver
        int driverSig = driverList.begin()->first;  // Key is the signature
        std::string driverName = driverList.begin()->second.second;
        std::cout << "[Main] Setting audio driver: " << driverName 
                  << " (sig=0x" << std::hex << driverSig << std::dec << ")" << std::endl;
        audioManager.set_driver(driverSig);
    } else {
        std::cout << "[Main] WARNING: No audio drivers available!" << std::endl;
    }
    
    std::cout << "[Main] Audio enabled: " << (audioManager.get_audio_enabled() ? "yes" : "no") << std::endl;
    std::cout << "[Main] Audio driver: " << audioManager.get_driver() << std::endl;
    std::cout << "[Main] Audio device: " << audioManager.get_device() << std::endl;
    
    UserConfig userConfig = LoadUserConfig();

    Window window;
    if (!window.Initialize(userConfig.windowWidth, userConfig.windowHeight, "MDSDRV Editor")) {
        return -1;
    }
    
    // Set window handle for audio manager (needed for some audio drivers)
    // CoreAudio (macOS) and ALSA/PulseAudio (Linux) don't need a window handle
    #ifndef __EMSCRIPTEN__
    audioManager.set_window_handle(nullptr);
    #endif

    Editor editor;

#ifdef __EMSCRIPTEN__
    // Disable file system access for Emscripten
    ImGui::GetIO().IniFilename = nullptr;
    
    // Emscripten main loop
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    // Native main loop
    while (true)
#endif
    {
        // Check close flag first, before processing any events
        // This ensures we exit immediately when close is requested
        if (window.ShouldClose()) {
            break;
        }
        
        window.BeginFrame();
        
        // Check again after processing events
        if (window.ShouldClose()) {
            break;
        }
        
        editor.Render();
        
        window.EndFrame();
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Stop any playback and clean up audio before shutting down window
    editor.StopMML();
    audioManager.clean_up();
    std::cout << "[Main] Audio_Manager cleaned up" << std::endl;
    
    window.Shutdown();
    return 0;
}

