include(FetchContent)

# Set policy for timestamp handling
cmake_policy(SET CMP0135 NEW)

# ONNX Runtime version
set(ONNXRUNTIME_VERSION "1.18.0")

# Platform-specific URLs and checksums
if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-arm64-${ONNXRUNTIME_VERSION}.tgz")
        # TODO: Add SHA256 checksum for security
        # set(ONNXRUNTIME_SHA256 "...")
    else()
        set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-osx-universal2-${ONNXRUNTIME_VERSION}.tgz")
        # TODO: Add SHA256 checksum for security
        # set(ONNXRUNTIME_SHA256 "...")
    endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-win-x64-${ONNXRUNTIME_VERSION}.zip")
    # TODO: Add SHA256 checksum for security
    # set(ONNXRUNTIME_SHA256 "...")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(ONNXRUNTIME_URL "https://github.com/microsoft/onnxruntime/releases/download/v${ONNXRUNTIME_VERSION}/onnxruntime-linux-x64-${ONNXRUNTIME_VERSION}.tgz")
    # TODO: Add SHA256 checksum for security
    # set(ONNXRUNTIME_SHA256 "...")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Declare the dependency
if(DEFINED ONNXRUNTIME_SHA256)
    FetchContent_Declare(
        onnxruntime
        URL ${ONNXRUNTIME_URL}
        URL_HASH SHA256=${ONNXRUNTIME_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
else()
    message(WARNING "ONNX Runtime checksum not verified - consider adding SHA256 for security")
    FetchContent_Declare(
        onnxruntime
        URL ${ONNXRUNTIME_URL}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
endif()

FetchContent_MakeAvailable(onnxruntime)

# Set variables for the main CMakeLists.txt
set(ONNXRUNTIME_ROOT_PATH ${onnxruntime_SOURCE_DIR})
set(ONNXRUNTIME_INCLUDE_DIRS ${onnxruntime_SOURCE_DIR}/include)
set(ONNXRUNTIME_LIBRARIES ${onnxruntime_SOURCE_DIR}/lib)