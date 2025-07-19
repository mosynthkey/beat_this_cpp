@echo off
echo Building Beat This! C++ for Windows...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring project...

REM Try to find vcpkg installation
if exist "C:\vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo Using vcpkg toolchain...
    cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
) else (
    echo vcpkg not found at C:\vcpkg - trying default paths...
    cmake .. -G "Visual Studio 17 2022" -A x64
)

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    echo.
    echo Make sure you have installed the required dependencies via vcpkg:
    echo.
    echo 1. Install vcpkg if not already installed:
    echo    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    echo    cd C:\vcpkg ^&^& .\bootstrap-vcpkg.bat
    echo.
    echo 2. Install dependencies:
    echo    C:\vcpkg\vcpkg.exe install fftw3 libsndfile soxr --triplet=x64-windows
    echo.
    echo See WINDOWS_BUILD.md for detailed instructions.
    pause
    exit /b 1
)

REM Build the project
echo Building project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Executables are in: build\Release\
echo.
pause