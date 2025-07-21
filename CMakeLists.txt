cmake_minimum_required(VERSION 3.10)
project(BeatThisCpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific compiler flags
if(WIN32)
    # Windows-specific settings
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        # Use UTF-8 encoding and disable some warnings
        add_compile_options(/utf-8 /wd4996)
        # Use static runtime
        # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    # Unix-like systems (macOS, Linux)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
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
    # Windows: Use vcpkg find_package
    find_package(SndFile CONFIG REQUIRED)
    
    # Set variables for compatibility
    set(SNDFILE_INCLUDE_DIRS ${SndFile_INCLUDE_DIRS})
    set(SNDFILE_LIBRARIES SndFile::sndfile)
    
    # Set soxr paths manually
    set(SOXR_INCLUDE_DIRS "C:/vcpkg/installed/x64-windows/include")
    set(SOXR_LIBRARIES "C:/vcpkg/installed/x64-windows/lib/soxr.lib")
else()
    # macOS and Linux: Use pkg-config
    find_package(PkgConfig REQUIRED)
    
    pkg_check_modules(SNDFILE REQUIRED sndfile)
    link_directories(/opt/homebrew/opt/libsndfile/lib)
    include_directories(/opt/homebrew/opt/libsndfile/include)

    # Manually set SOXR paths for Homebrew installation
    set(SOXR_INCLUDE_DIRS /opt/homebrew/opt/libsoxr/include)
    set(SOXR_LIBRARIES /opt/homebrew/opt/libsoxr/lib/libsoxr.dylib)
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
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    )
endif()

# Check if all required dependencies were found
# find_package will throw an error if dependencies are not found

target_include_directories(beat_this_api PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/Submodule/pocketfft # Add PocketFFT include directory
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For header files
)

target_link_libraries(beat_this_api onnxruntime ${SNDFILE_LIBRARIES} ${SOXR_LIBRARIES})

if(WIN32)
    add_custom_command(
        TARGET beat_this_api POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_LIBRARIES}/onnxruntime.dll"
        "$<TARGET_FILE_DIR:beat_this_api>"
        COMMENT "Copying onnxruntime.dll to API DLL directory"
    )
endif()

# Main executable with integrated beat analysis and audio generation
add_executable(beat_this_cpp Source/main.cpp)

target_include_directories(beat_this_cpp PRIVATE
    ${ONNXRUNTIME_INCLUDE_DIRS} # For main.cpp to include onnxruntime_cxx_api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Source # For beat_this_api.h
    ${SNDFILE_INCLUDE_DIRS}
    ${SOXR_INCLUDE_DIRS}
)

target_link_libraries(beat_this_cpp beat_this_api ${SNDFILE_LIBRARIES})

if(WIN32)
    add_custom_command(
        TARGET beat_this_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_LIBRARIES}/onnxruntime.dll"
        "$<TARGET_FILE_DIR:beat_this_cpp>"
        COMMENT "Copying onnxruntime.dll to executable directory"
    )
endif()
