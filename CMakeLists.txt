cmake_minimum_required(VERSION 3.15)
project(mdsdrv-editor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

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

# ImGui sources
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${IMGUI_SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    glfw
    OpenGL::GL
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

