cmake_minimum_required(VERSION 3.15)
project(mdsdrv-editor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "Initializing top-level submodules…")

execute_process(
    COMMAND git submodule update --init deps/imgui
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

execute_process(
    COMMAND git submodule update --init deps/mmlgui
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

message(STATUS "Initializing nested mmlgui submodules…")

execute_process(
    COMMAND sh -c "cd deps/mmlgui && git submodule update --init MDSDRV"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

execute_process(
    COMMAND sh -c "cd deps/mmlgui && git submodule update --init ctrmml"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

execute_process(
    COMMAND sh -c "cd deps/mmlgui && git submodule update --init imgui"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

execute_process(
    COMMAND sh -c "cd deps/mmlgui && git submodule update --init libvgm"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

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
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Generate embedded mdsdrv.bin include at build time
set(MDSDRV_BIN_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/deps/mmlgui/MDSDRV/out/mdsdrv.bin)
set(MDSDRV_BIN_INC ${CMAKE_CURRENT_BINARY_DIR}/mdsdrv_bin.inc)
add_custom_command(
    OUTPUT ${MDSDRV_BIN_INC}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND xxd -i -n mdsdrv_bin ${MDSDRV_BIN_SOURCE} > ${MDSDRV_BIN_INC}
    DEPENDS ${MDSDRV_BIN_SOURCE}
    COMMENT "Generating embedded mdsdrv.bin include"
)
add_custom_target(mdsdrv_bin_inc DEPENDS ${MDSDRV_BIN_INC})

# Source files
set(SOURCES
    main.cpp
    window.cpp
    editor.cpp
    theme.cpp
    config.cpp
    export_window.cpp
    mdsbin_export_window.cpp
    mdsdrv_bin.cpp
    pcm_tool_window.cpp
    pattern_editor.cpp
)

set(HEADERS
    window.h
    editor.h
    theme.h
    config.h
    export_window.h
    mdsbin_export_window.h
    mdsdrv_bin.h
    pcm_tool_window.h
    pattern_editor.h
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

# LibVGM utils sources - needed for OSMutex, OSSignal, and OSThread
# PulseAudio and ALSA drivers require OSSignal and OSThread
if(APPLE OR UNIX)
    set(LIBVGM_UTILS_SOURCES
        ${LIBVGM_DIR}/utils/OSMutex_POSIX.c
        ${LIBVGM_DIR}/utils/OSSignal_POSIX.c
        ${LIBVGM_DIR}/utils/OSThread_POSIX.c
    )
elseif(WIN32)
    set(LIBVGM_UTILS_SOURCES
        ${LIBVGM_DIR}/utils/OSMutex_Win.c
        ${LIBVGM_DIR}/utils/OSSignal_Win.c
        ${LIBVGM_DIR}/utils/OSThread_Win.c
    )
endif()

# LibVGM audio sources - minimal set for macOS
set(LIBVGM_AUDIO_SOURCES
    ${LIBVGM_DIR}/audio/AudioStream.c
)

# Force C99 for libvgm C files
set_source_files_properties(
    deps/mmlgui/libvgm/audio/AudioStream.c
    deps/mmlgui/libvgm/emu/SoundEmu.c
    deps/mmlgui/libvgm/emu/Resampler.c
    deps/mmlgui/libvgm/emu/logging.c
    deps/mmlgui/libvgm/emu/panning.c
    deps/mmlgui/libvgm/emu/dac_control.c
    deps/mmlgui/libvgm/emu/cores/sn764intf.c
    deps/mmlgui/libvgm/emu/cores/sn76496.c
    deps/mmlgui/libvgm/emu/cores/2612intf.c
    deps/mmlgui/libvgm/emu/cores/fmopn.c
    PROPERTIES
    LANGUAGE C
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)


# Add platform-specific audio driver (CoreAudio for macOS, ALSA/PulseAudio for Linux)
if(APPLE AND NOT IS_WASM_BUILD)
    set(LIBVGM_AUDIO_SOURCES ${LIBVGM_AUDIO_SOURCES}
        ${LIBVGM_DIR}/audio/AudDrv_CoreAudio.c
    )
    add_definitions(-DAUDDRV_CA)
elseif(WIN32)
    # Windows drivers would go here
elseif(UNIX AND NOT APPLE)
    # Linux drivers - try PulseAudio first, then ALSA
    set(LINUX_AUDIO_LIBS "")  # Initialize to empty
    find_package(PkgConfig QUIET)
    
    # Try to find PulseAudio using pkg-config (more reliable on Linux)
    set(PULSEAUDIO_FOUND FALSE)
    if(PkgConfig_FOUND)
        pkg_check_modules(PULSEAUDIO QUIET libpulse-simple)
        if(PULSEAUDIO_FOUND)
            # Also need the main pulse library
            pkg_check_modules(PULSE QUIET libpulse)
        endif()
    endif()
    
    # Fallback to CMake's FindPulseAudio if pkg-config didn't work
    if(NOT PULSEAUDIO_FOUND)
        find_package(PulseAudio QUIET)
    endif()
    
    find_package(ALSA QUIET)
    
    # Prefer PulseAudio if available, otherwise use ALSA
    if(PULSEAUDIO_FOUND)
        set(LIBVGM_AUDIO_SOURCES ${LIBVGM_AUDIO_SOURCES}
            ${LIBVGM_DIR}/audio/AudDrv_Pulse.c
        )
        add_definitions(-DAUDDRV_PULSE)
        if(PULSEAUDIO_INCLUDE_DIRS)
            include_directories(${PULSEAUDIO_INCLUDE_DIRS})
        elseif(PULSEAUDIO_INCLUDE_DIR)
            include_directories(${PULSEAUDIO_INCLUDE_DIR})
        endif()
        # PulseAudio needs both pulse-simple and the main library
        if(PULSEAUDIO_LIBRARIES)
            set(LINUX_AUDIO_LIBS ${PULSEAUDIO_LIBRARIES})
            if(PULSE_LIBRARIES)
                list(APPEND LINUX_AUDIO_LIBS ${PULSE_LIBRARIES})
            endif()
        elseif(PULSEAUDIO_LIBRARY)
            set(LINUX_AUDIO_LIBS pulse-simple ${PULSEAUDIO_LIBRARY})
        else()
            set(LINUX_AUDIO_LIBS pulse-simple pulse)
        endif()
    elseif(ALSA_FOUND)
        set(LIBVGM_AUDIO_SOURCES ${LIBVGM_AUDIO_SOURCES}
            ${LIBVGM_DIR}/audio/AudDrv_ALSA.c
        )
        add_definitions(-DAUDDRV_ALSA)
        include_directories(${ALSA_INCLUDE_DIRS})
        set(LINUX_AUDIO_LIBS ${ALSA_LIBRARIES})
    else()
        message(WARNING "No audio drivers found for Linux. Please install ALSA or PulseAudio development packages.")
        message(WARNING "  For PulseAudio: sudo apt-get install libpulse-dev (Debian/Ubuntu)")
        message(WARNING "  For ALSA: sudo apt-get install libasound2-dev (Debian/Ubuntu)")
    endif()
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
    add_dependencies(${PROJECT_NAME}.html mdsdrv_bin_inc)
    
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
    add_dependencies(${PROJECT_NAME} mdsdrv_bin_inc)
    
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
            ${LINUX_AUDIO_LIBS}
        )
    endif()
endif()

# Apply MinGW workaround: include <cstdint> for all C++ files only
if(MINGW)
    # Collect all C++ sources from your target
    get_target_property(PROJECT_SOURCES ${PROJECT_NAME} SOURCES)

    foreach(src ${PROJECT_SOURCES})
        get_filename_component(ext ${src} EXT)
        if(ext STREQUAL ".cpp")
            set_source_files_properties(${src} PROPERTIES COMPILE_OPTIONS "-include;cstdint")
        endif()
    endforeach()
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

