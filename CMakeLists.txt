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

# Source files
set(SOURCES
    main.cpp
    window.cpp
    editor.cpp
    theme.cpp
)

set(HEADERS
    window.h
    editor.h
    theme.h
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

# Create executable
if(IS_WASM_BUILD)
    # WASM build - use .html extension so Emscripten generates HTML file with shell template
    add_executable(${PROJECT_NAME}.html ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})
    
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
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})
    
    # Find packages for native build
    find_package(glfw3 REQUIRED)
    find_package(OpenGL REQUIRED)
    
    # Link libraries for native
    target_link_libraries(${PROJECT_NAME}
        glfw
        OpenGL::GL
    )
endif()

# Include directories (common for both)
if(IS_WASM_BUILD)
    target_include_directories(${PROJECT_NAME}.html PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
else()
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
endif()

