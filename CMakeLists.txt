cmake_minimum_required(VERSION 3.15)
project(mdsdrv-editor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check if building for WebAssembly
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten" OR EMSCRIPTEN)
    set(IS_WASM_BUILD TRUE)
    message(STATUS "Building for WebAssembly")
else()
    set(IS_WASM_BUILD FALSE)
    message(STATUS "Building for native platform")
endif()

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui)
include_directories(${IMGUI_DIR})

# MMLGUI
set(MMLGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps/mmlgui)
set(LIBVGM_DIR ${MMLGUI_DIR}/libvgm)
include_directories(${MMLGUI_DIR}/src)
include_directories(${MMLGUI_DIR}/ctrmml/src)
include_directories(${LIBVGM_DIR})
include_directories(${LIBVGM_DIR}/audio)
include_directories(${LIBVGM_DIR}/emu)
include_directories(${LIBVGM_DIR}/utils)

# Source files
set(SOURCES
    main.cpp
    window.cpp
    editor.cpp
    theme.cpp
    export_window.cpp
    pcm_tool_window.cpp
)

set(HEADERS
    window.h
    editor.h
    theme.h
    export_window.h
    pcm_tool_window.h
)

# ImGui sources - common files
file(GLOB IMGUI_CORE_SOURCES
    "${IMGUI_DIR}/*.cpp"
)

# Backend sources depend on build target
if(IS_WASM_BUILD)
    # For WASM, use SDL2 backend (or GLFW if available with Emscripten)
    # GLFW works with Emscripten, so we can use it
    set(IMGUI_BACKEND_SOURCES
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )
else()
    # Native build uses GLFW
    set(IMGUI_BACKEND_SOURCES
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )
endif()

set(IMGUI_SOURCES ${IMGUI_CORE_SOURCES} ${IMGUI_BACKEND_SOURCES})

# ImGui file system addon (for file dialogs)
set(IMGUI_FILESYSTEM_SOURCES
    ${MMLGUI_DIR}/imgui/addons/imguifilesystem/imguifilesystem.cpp
)

# MMLGUI sources - core files needed for playback
set(MMLGUI_SOURCES
    ${MMLGUI_DIR}/src/song_manager.cpp
    ${MMLGUI_DIR}/src/emu_player.cpp
    ${MMLGUI_DIR}/src/audio_manager.cpp
    ${MMLGUI_DIR}/src/track_info.cpp
)

# CTRMML sources - needed by song_manager
set(CTRMML_SOURCES
    ${MMLGUI_DIR}/ctrmml/src/song.cpp
    ${MMLGUI_DIR}/ctrmml/src/input.cpp
    ${MMLGUI_DIR}/ctrmml/src/player.cpp
    ${MMLGUI_DIR}/ctrmml/src/mml_input.cpp
    ${MMLGUI_DIR}/ctrmml/src/driver.cpp
    ${MMLGUI_DIR}/ctrmml/src/track.cpp
    ${MMLGUI_DIR}/ctrmml/src/vgm.cpp
    ${MMLGUI_DIR}/ctrmml/src/conf.cpp
    ${MMLGUI_DIR}/ctrmml/src/optimizer.cpp
    ${MMLGUI_DIR}/ctrmml/src/riff.cpp
    ${MMLGUI_DIR}/ctrmml/src/wave.cpp
    ${MMLGUI_DIR}/ctrmml/src/stringf.cpp
)

# Platform-specific files (MD platform) - exclude mdslink.cpp as it has its own main()
file(GLOB CTRMML_PLATFORM_SOURCES
    "${MMLGUI_DIR}/ctrmml/src/platform/md.cpp"
    "${MMLGUI_DIR}/ctrmml/src/platform/mdsdrv.cpp"
)

# LibVGM utils sources - needed for OSMutex
if(APPLE OR UNIX)
    set(LIBVGM_UTILS_SOURCES
        ${LIBVGM_DIR}/utils/OSMutex_POSIX.c
    )
elseif(WIN32)
    set(LIBVGM_UTILS_SOURCES
        ${LIBVGM_DIR}/utils/OSMutex_Win.c
    )
endif()

# LibVGM audio sources - minimal set for macOS
set(LIBVGM_AUDIO_SOURCES
    ${LIBVGM_DIR}/audio/AudioStream.c
)

# Add platform-specific audio driver (CoreAudio for macOS)
if(APPLE AND NOT IS_WASM_BUILD)
    set(LIBVGM_AUDIO_SOURCES ${LIBVGM_AUDIO_SOURCES}
        ${LIBVGM_DIR}/audio/AudDrv_CoreAudio.c
    )
    add_definitions(-DAUDDRV_CA)
elseif(WIN32)
    # Windows drivers would go here
elseif(UNIX AND NOT APPLE)
    # Linux drivers would go here
endif()

# LibVGM emu sources - minimal set needed for SN76496 and YM2612
set(LIBVGM_EMU_SOURCES
    ${LIBVGM_DIR}/emu/SoundEmu.c
    ${LIBVGM_DIR}/emu/Resampler.c
    ${LIBVGM_DIR}/emu/logging.c
    ${LIBVGM_DIR}/emu/panning.c
    ${LIBVGM_DIR}/emu/dac_control.c
    # SN76496 core (needed for MDSDRV)
    ${LIBVGM_DIR}/emu/cores/sn764intf.c
    ${LIBVGM_DIR}/emu/cores/sn76496.c
    # YM2612 core (needed for MDSDRV)
    ${LIBVGM_DIR}/emu/cores/2612intf.c
    ${LIBVGM_DIR}/emu/cores/fmopn.c
)

# Add compile definitions for libvgm
add_definitions(-DSNDDEV_SELECT -DSNDDEV_SN76496 -DEC_SN76496_MAME -DSNDDEV_YM2612 -DEC_YM2612_GPGX)

# Create executable
if(IS_WASM_BUILD)
    # WASM build - use .html extension so Emscripten generates HTML file with shell template
    add_executable(${PROJECT_NAME}.html ${SOURCES} ${HEADERS} ${IMGUI_SOURCES} ${IMGUI_FILESYSTEM_SOURCES} ${MMLGUI_SOURCES} ${CTRMML_SOURCES} ${CTRMML_PLATFORM_SOURCES} ${LIBVGM_AUDIO_SOURCES} ${LIBVGM_EMU_SOURCES} ${LIBVGM_UTILS_SOURCES})
    
    # Set the executable suffix to empty so we get .html output
    set_target_properties(${PROJECT_NAME}.html PROPERTIES
        SUFFIX ""
    )
    
    # Emscripten-specific settings
    # Check if shell.html exists, otherwise use default
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/wasm-template/shell.html)
        set(SHELL_FILE_OPTION "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/wasm-template/shell.html")
    else()
        set(SHELL_FILE_OPTION "")
    endif()
    
    # Compile options - only optimization flags here
    target_compile_options(${PROJECT_NAME}.html PRIVATE
        -O2
    )
    
    # Link options - all Emscripten -s flags go here
    # Use set_target_properties with LINK_FLAGS for proper Emscripten flag handling
    set(EMSCRIPTEN_LINK_FLAGS_LIST
        "-sUSE_GLFW=3"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sUSE_WEBGL2=1"
        "-sFULL_ES3=1"
        "-sEXPORTED_RUNTIME_METHODS=[\"ccall\",\"cwrap\"]"
    )
    
    string(JOIN " " EMSCRIPTEN_LINK_FLAGS ${EMSCRIPTEN_LINK_FLAGS_LIST})
    
    set_target_properties(${PROJECT_NAME}.html PROPERTIES
        LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
    )
    
    # Shell file needs to be set separately using LINK_OPTIONS
    # Also explicitly set output to .html so Emscripten generates the HTML file
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/wasm-template/shell.html)
        get_filename_component(SHELL_FILE_ABS ${CMAKE_CURRENT_SOURCE_DIR}/wasm-template/shell.html ABSOLUTE)
        target_link_options(${PROJECT_NAME}.html PRIVATE
            "--shell-file" "${SHELL_FILE_ABS}"
            "-o" "${PROJECT_NAME}.html"
        )
    else()
        target_link_options(${PROJECT_NAME}.html PRIVATE
            "-o" "${PROJECT_NAME}.html"
        )
    endif()
    
    # For WASM, we don't need to find packages the same way
    # Emscripten provides GLFW and OpenGL through its ports
else()
    # Native build
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES} ${IMGUI_FILESYSTEM_SOURCES} ${MMLGUI_SOURCES} ${CTRMML_SOURCES} ${CTRMML_PLATFORM_SOURCES} ${LIBVGM_AUDIO_SOURCES} ${LIBVGM_EMU_SOURCES} ${LIBVGM_UTILS_SOURCES})
    
    # Find packages for native build
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    
    # Link libraries for native
    if(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            glfw
            OpenGL::GL
            "-framework AudioToolbox"
        )
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE
            glfw
            OpenGL::GL
        )
    endif()
endif()

# Define LOCAL_LIBVGM to use local libvgm headers
add_definitions(-DLOCAL_LIBVGM)

# Define IMGUI_DEFINE_MATH_OPERATORS for ImGui file system addon
add_definitions(-DIMGUI_DEFINE_MATH_OPERATORS)

# Include directories (common for both)
if(IS_WASM_BUILD)
    target_include_directories(${PROJECT_NAME}.html PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${MMLGUI_DIR}/imgui/addons/imguifilesystem
        ${MMLGUI_DIR}/src
        ${MMLGUI_DIR}/ctrmml/src
        ${LIBVGM_DIR}
        ${LIBVGM_DIR}/audio
        ${LIBVGM_DIR}/emu
        ${LIBVGM_DIR}/utils
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${MMLGUI_DIR}/imgui/addons/imguifilesystem
        ${MMLGUI_DIR}/src
        ${MMLGUI_DIR}/ctrmml/src
        ${LIBVGM_DIR}
        ${LIBVGM_DIR}/audio
        ${LIBVGM_DIR}/emu
        ${LIBVGM_DIR}/utils
    )
endif()

