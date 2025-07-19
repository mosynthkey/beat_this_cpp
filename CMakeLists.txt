cmake_minimum_required(VERSION 3.10)
project(BeatThisCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific compiler flags
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        # Use UTF-8 encoding and disable some warnings
        add_compile_options(/utf-8 /wd4996)
    endif()
else()
    # Unix-like systems (macOS, Linux)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

# --- FFTW --- #
if(WIN32)
    # Windows: Use vcpkg or manual installation
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFTW3 fftw3f)
    endif()
    
    if(NOT FFTW3_FOUND)
        # Try to find FFTW manually on Windows
        find_path(FFTW3_INCLUDE_DIR fftw3.h
            HINTS 
                ${CMAKE_PREFIX_PATH}/include
                $ENV{FFTW_ROOT}/include
                "C:/vcpkg/installed/x64-windows/include"
                "C:/Program Files/fftw/include"
        )
        
        find_library(FFTW3_LIBRARIES
            NAMES fftw3f libfftw3f fftw3f-3
            HINTS 
                ${CMAKE_PREFIX_PATH}/lib
                $ENV{FFTW_ROOT}/lib
                "C:/vcpkg/installed/x64-windows/lib"
                "C:/Program Files/fftw/lib"
        )
    endif()
elseif(APPLE)
    # Add Homebrew library path for macOS
    link_directories(/opt/homebrew/lib)
    set(FFTW3_INCLUDE_DIR /opt/homebrew/opt/fftw/include)
    set(FFTW3_LIBRARIES /opt/homebrew/opt/fftw/lib/libfftw3f.dylib)
else()
    # Linux: Use pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFTW3 REQUIRED fftw3f)
endif()

# --- ONNX Runtime --- #
# Option to use system ONNX Runtime or fetch automatically
option(USE_SYSTEM_ONNXRUNTIME "Use system-installed ONNX Runtime" OFF)

if(USE_SYSTEM_ONNXRUNTIME)
    find_package(onnxruntime REQUIRED)
else()
    # Fetch ONNX Runtime automatically
    include(cmake/FetchONNXRuntime.cmake)
    
    # Set up ONNX Runtime paths
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        if(CMAKE_SYSTEM_NAME MATCHES "iOS")
            set(ONNXRUNTIME_LIB ${ONNXRUNTIME_LIBRARIES}/libonnxruntime.a)
        else()
            set(ONNXRUNTIME_LIB ${ONNXRUNTIME_LIBRARIES}/libonnxruntime.1.18.0.dylib)
        endif()
    elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
        set(ONNXRUNTIME_LIB ${ONNXRUNTIME_LIBRARIES}/onnxruntime.lib)
        set(ONNXRUNTIME_DLL ${ONNXRUNTIME_LIBRARIES}/onnxruntime.dll)
    else()
        set(ONNXRUNTIME_LIB ${ONNXRUNTIME_LIBRARIES}/libonnxruntime.so.1.18.0)
    endif()
    
    add_library(onnxruntime SHARED IMPORTED)
    if(WIN32)
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION ${ONNXRUNTIME_DLL}
            IMPORTED_IMPLIB ${ONNXRUNTIME_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${ONNXRUNTIME_INCLUDE_DIRS})
    else()
        set_target_properties(onnxruntime PROPERTIES
            IMPORTED_LOCATION ${ONNXRUNTIME_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${ONNXRUNTIME_INCLUDE_DIRS})
    endif()
endif()

# --- Find Dependencies --- #
if(WIN32)
    # Windows: Try to find libraries manually or via vcpkg
    find_package(PkgConfig QUIET)
    
    # Find sndfile
    if(PkgConfig_FOUND)
        pkg_check_modules(SNDFILE sndfile)
    endif()
    
    if(NOT SNDFILE_FOUND)
        find_path(SNDFILE_INCLUDE_DIRS sndfile.h
            HINTS 
                ${CMAKE_PREFIX_PATH}/include
                $ENV{SNDFILE_ROOT}/include
                "C:/vcpkg/installed/x64-windows/include"
                "C:/Program Files/libsndfile/include"
        )
        
        find_library(SNDFILE_LIBRARIES
            NAMES sndfile libsndfile sndfile-1
            HINTS 
                ${CMAKE_PREFIX_PATH}/lib
                $ENV{SNDFILE_ROOT}/lib
                "C:/vcpkg/installed/x64-windows/lib"
                "C:/Program Files/libsndfile/lib"
        )
    endif()
    
    # Find soxr
    if(PkgConfig_FOUND)
        pkg_check_modules(SOXR soxr)
    endif()
    
    if(NOT SOXR_FOUND)
        find_path(SOXR_INCLUDE_DIRS soxr.h
            HINTS 
                ${CMAKE_PREFIX_PATH}/include
                $ENV{SOXR_ROOT}/include
                "C:/vcpkg/installed/x64-windows/include"
                "C:/Program Files/soxr/include"
        )
        
        find_library(SOXR_LIBRARIES
            NAMES soxr libsoxr
            HINTS 
                ${CMAKE_PREFIX_PATH}/lib
                $ENV{SOXR_ROOT}/lib
                "C:/vcpkg/installed/x64-windows/lib"
                "C:/Program Files/soxr/lib"
        )
    endif()
else()
    # macOS and Linux: Use pkg-config
    find_package(PkgConfig REQUIRED)
    
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    pkg_check_modules(SOXR REQUIRED soxr)
endif()

add_library(beat_this_api SHARED 
    Source/beat_this_api.cpp 
    Source/MelSpectrogram.cpp 
    Source/InferenceProcessor.cpp 
    Source/Postprocessor.cpp)

# Windows-specific settings for DLL export
if(WIN32)
    set_target_properties(beat_this_api PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Release
    )
endif()

# Check if all required dependencies were found
if(WIN32)
    if(NOT FFTW3_INCLUDE_DIR AND NOT FFTW3_INCLUDE_DIRS)
        message(FATAL_ERROR "FFTW3 not found! Please install via vcpkg: vcpkg install fftw3")
    endif()
    if(NOT SNDFILE_INCLUDE_DIRS)
        message(FATAL_ERROR "libsndfile not found! Please install via vcpkg: vcpkg install libsndfile")
    endif()
    if(NOT SOXR_INCLUDE_DIRS)
        message(FATAL_ERROR "libsoxr not found! Please install via vcpkg: vcpkg install libsoxr")
    endif()
    if(NOT FFTW3_LIBRARIES)
        message(FATAL_ERROR "FFTW3 libraries not found! Please install via vcpkg: vcpkg install fftw3")
    endif()
    if(NOT SNDFILE_LIBRARIES)
        message(FATAL_ERROR "libsndfile libraries not found! Please install via vcpkg: vcpkg install libsndfile")
    endif()
    if(NOT SOXR_LIBRARIES)
        message(FATAL_ERROR "libsoxr libraries not found! Please install via vcpkg: vcpkg install libsoxr")
    endif()
endif()

target_include_directories(beat_this_api PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For header files
)

target_link_libraries(beat_this_api onnxruntime ${SNDFILE_LIBRARIES} ${SOXR_LIBRARIES} ${FFTW3_LIBRARIES})

# Main executable with integrated beat analysis and audio generation
add_executable(beat_this_cpp Source/main.cpp)

target_include_directories(beat_this_cpp PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS} # For main.cpp to include onnxruntime_cxx_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For beat_this_api.h
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)

target_link_libraries(beat_this_cpp beat_this_api ${SNDFILE_LIBRARIES})
