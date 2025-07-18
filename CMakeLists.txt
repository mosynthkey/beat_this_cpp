cmake_minimum_required(VERSION 3.10)
project(BeatThisCpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

# Add Homebrew library path for macOS
if(APPLE)
    link_directories(/opt/homebrew/lib)
endif()

# --- FFTW --- #
set(FFTW3_INCLUDE_DIR /opt/homebrew/opt/fftw/include)
set(FFTW3_LIBRARIES /opt/homebrew/opt/fftw/lib/libfftw3f.dylib)

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
    else()
        set(ONNXRUNTIME_LIB ${ONNXRUNTIME_LIBRARIES}/libonnxruntime.so.1.18.0)
    endif()
    
    add_library(onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime PROPERTIES
        IMPORTED_LOCATION ${ONNXRUNTIME_LIB}
        INTERFACE_INCLUDE_DIRECTORIES ${ONNXRUNTIME_INCLUDE_DIRS})
endif()

# --- Find Dependencies with Pkg-config --- #
find_package(PkgConfig REQUIRED)

pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(SOXR REQUIRED soxr)

add_library(beat_this_api SHARED 
    Source/beat_this_api.cpp 
    Source/MelSpectrogram.cpp 
    Source/InferenceProcessor.cpp 
    Source/Postprocessor.cpp)

target_include_directories(beat_this_api PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For header files
)

target_link_libraries(beat_this_api onnxruntime ${SNDFILE_LIBRARIES} ${SOXR_LIBRARIES} ${FFTW3_LIBRARIES})

# Example executable for testing the API
add_executable(beat_this_cpp Source/main.cpp)

# New executable for beats to audio conversion
add_executable(beats_to_audio Source/beats_to_audio.cpp)

target_include_directories(beat_this_cpp PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS} # For main.cpp to include onnxruntime_cxx_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For beat_this_api.h
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIR}
)

target_link_libraries(beat_this_cpp beat_this_api ${SNDFILE_LIBRARIES})

target_link_libraries(beats_to_audio ${SNDFILE_LIBRARIES})

target_include_directories(beats_to_audio PRIVATE
    ${SNDFILE_INCLUDE_DIRS}
)
