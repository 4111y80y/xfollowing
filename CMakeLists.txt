cmake_minimum_required(VERSION 3.16)

project(xfollowing VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check compiler - CEF requires MSVC
if(NOT MSVC)
    message(WARNING
        "CEF libraries are compiled with MSVC. Using ${CMAKE_CXX_COMPILER_ID} may cause link errors. "
        "Recommend: Select 'Desktop Qt 6.10.1 MSVC 2022 64bit' kit in Qt Creator.")
endif()

# Qt configuration
if(NOT CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "D:/Qt/6.10.1/msvc2022_64")
endif()
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Network)

# CEF configuration
set(CEF_ROOT "D:/GitHub/cef")
set(CEF_INCLUDE_PATH "${CEF_ROOT}" "${CEF_ROOT}/include")

# Source files
set(PROJECT_SOURCES
    src/main.cpp
    # App
    src/App/CefApp.h
    src/App/CefApp.cpp
    src/App/CefHandler.h
    src/App/CefHandler.cpp
    # UI
    src/UI/MainWindow.h
    src/UI/MainWindow.cpp
    src/UI/BrowserWidget.h
    src/UI/BrowserWidget.cpp
    src/UI/PostListPanel.h
    src/UI/PostListPanel.cpp
    src/UI/KeywordPanel.h
    src/UI/KeywordPanel.cpp
    # Data
    src/Data/Post.h
    src/Data/Keyword.h
    src/Data/DataStorage.h
    src/Data/DataStorage.cpp
    # Core
    src/Core/PostMonitor.h
    src/Core/PostMonitor.cpp
    src/Core/AutoFollower.h
    src/Core/AutoFollower.cpp
    # Utils
    src/Utils/Logger.h
    src/Utils/Logger.cpp
)

# Create executable
qt_add_executable(xfollowing
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Include directories
target_include_directories(xfollowing PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CEF_INCLUDE_PATH}
)

# Compile definitions for CEF
target_compile_definitions(xfollowing PRIVATE
    WIN32
    _WIN32
    _WINDOWS
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# Link Qt libraries
target_link_libraries(xfollowing PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Network
)

# CEF libraries
target_link_libraries(xfollowing PRIVATE
    "${CEF_ROOT}/Release/libcef.lib"
    "${CEF_ROOT}/build/libcef_dll_wrapper/Release/libcef_dll_wrapper.lib"
)

# Windows system libraries required by CEF
target_link_libraries(xfollowing PRIVATE
    comctl32
    rpcrt4
    shlwapi
    ws2_32
    delayimp
)

# Linker flags for delayed loading
if(MSVC)
    target_link_options(xfollowing PRIVATE
        /DELAYLOAD:libcef.dll
    )
    # Use UTF-8 source and execution charset
    target_compile_options(xfollowing PRIVATE /utf-8)
endif()

# Windows specific settings
set_target_properties(xfollowing PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Copy CEF resources after build
add_custom_command(TARGET xfollowing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Release"
        $<TARGET_FILE_DIR:xfollowing>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CEF_ROOT}/Resources"
        $<TARGET_FILE_DIR:xfollowing>
)

# Run windeployqt to copy Qt DLLs
if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET xfollowing POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                $<TARGET_FILE:xfollowing>
            COMMENT "Running windeployqt to deploy Qt DLLs..."
        )
    endif()
endif()

qt_finalize_executable(xfollowing)
