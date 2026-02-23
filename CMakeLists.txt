cmake_minimum_required(VERSION 3.15)

project(shape_gen VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Find OpenGL (required for wxWidgets GL and GLAD)
# ============================================================================
find_package(OpenGL REQUIRED)

# ============================================================================
# wxWidgets Configuration
# ============================================================================
set(wxBUILD_SHARED OFF CACHE BOOL "Build wx libraries as shared libs")
set(wxBUILD_SAMPLES OFF CACHE BOOL "Build wx samples")
set(wxBUILD_TESTS OFF CACHE BOOL "Build wx tests")
set(wxBUILD_BENCHMARKS OFF CACHE BOOL "Build wx benchmarks")
set(wxBUILD_DEMOS OFF CACHE BOOL "Build wx demos")

set(wxUSE_OPENGL ON CACHE BOOL "Enable OpenGL support")
set(wxUSE_GLCANVAS ON CACHE BOOL "Enable GLCanvas")

add_subdirectory(external/wxWidgets EXCLUDE_FROM_ALL)

# ============================================================================
# GLAD - OpenGL Loader (Modern OpenGL 3.3+ Core Profile)
# ============================================================================
add_subdirectory(external/glad)

# ============================================================================
# GLM - OpenGL Mathematics Library
# ============================================================================
add_subdirectory(external/glm)

# ============================================================================
# Assimp - Asset Import Library (3D Model Loading)
# ============================================================================
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable Assimp tests")
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable Assimp tools")
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "Disable Assimp samples")
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable Assimp install")
set(ASSIMP_NO_EXPORT ON CACHE BOOL "Disable export functionality")

add_subdirectory(external/assimp EXCLUDE_FROM_ALL)

# ============================================================================
# RtAudio - Real-time Audio I/O
# ============================================================================
set(RTAUDIO_TARGETNAME_UNINSTALL "rtaudio_uninstall" CACHE STRING "Rename to avoid conflict" FORCE)
set(RTAUDIO_API_JACK OFF CACHE BOOL "Disable JACK" FORCE)
set(RTAUDIO_API_PULSE OFF CACHE BOOL "Disable PulseAudio" FORCE)
set(RTAUDIO_API_ALSA OFF CACHE BOOL "Disable ALSA" FORCE)
set(RTAUDIO_API_OSS OFF CACHE BOOL "Disable OSS" FORCE)
set(RTAUDIO_API_ASIO OFF CACHE BOOL "Disable ASIO" FORCE)
set(RTAUDIO_API_WASAPI OFF CACHE BOOL "Disable WASAPI" FORCE)
set(RTAUDIO_API_DS OFF CACHE BOOL "Disable DirectSound" FORCE)

if(APPLE)
    set(RTAUDIO_API_CORE ON CACHE BOOL "Use CoreAudio on macOS" FORCE)
elseif(WIN32)
    set(RTAUDIO_API_WASAPI ON CACHE BOOL "Use WASAPI on Windows" FORCE)
    set(RTAUDIO_API_DS ON CACHE BOOL "Use DirectSound on Windows" FORCE)
elseif(UNIX)
    set(RTAUDIO_API_ALSA ON CACHE BOOL "Use ALSA on Linux" FORCE)
    set(RTAUDIO_API_PULSE ON CACHE BOOL "Use PulseAudio on Linux" FORCE)
endif()

add_subdirectory(external/rtaudio EXCLUDE_FROM_ALL)

# ============================================================================
# libtorch - In-App Neural Network Training
# ============================================================================
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/libtorch" ${CMAKE_PREFIX_PATH})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
find_package(Torch REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

# ============================================================================
# Main Application
# ============================================================================
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainFrame.cpp
    src/MainFrame.h
    src/GLCanvas.cpp
    src/GLCanvas.h
    src/AudioEngine.cpp
    src/AudioEngine.h
    src/ModelLoader.cpp
    src/ModelLoader.h
    src/MeshBounds.cpp
    src/MeshBounds.h
    src/ShapeTrainer.cpp
    src/ShapeTrainer.h
    src/ExploreFrame.cpp
    src/ExploreFrame.h
    src/ExploreCanvas.cpp
    src/ExploreCanvas.h
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        # wxWidgets
        wx::core
        wx::base
        wx::gl

        # OpenGL
        OpenGL::GL

        # GLAD (OpenGL loader)
        glad

        # GLM (math library)
        glm::glm

        # Assimp (model loading)
        assimp

        # RtAudio
        rtaudio

        # libtorch
        "${TORCH_LIBRARIES}"
)

# Platform-specific configurations
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    )

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            "-framework CoreAudio"
            "-framework CoreFoundation"
    )

    # Ensure Frameworks dir exists before copying
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/")

    file(GLOB TORCH_DYLIBS "${CMAKE_CURRENT_SOURCE_DIR}/external/libtorch/lib/*.dylib")
    foreach(DYLIB ${TORCH_DYLIBS})
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DYLIB}"
                "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Frameworks/")
    endforeach()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND install_name_tool -add_rpath "@executable_path/../Frameworks"
            "$<TARGET_FILE:${PROJECT_NAME}>")
elseif(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall -Wextra -Wpedantic
    )
endif()

# ============================================================================
# Copy assets to build directory (for future use)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            ${CMAKE_BINARY_DIR}/bin/assets
        COMMENT "Copying assets to build directory"
    )
endif()
