@echo off
REM Lithium Browser Engine Build Script for Windows
REM Usage: build.bat [options]
REM   -c, --clean       Clean build (remove build directory first)
REM   -d, --debug       Build with Debug configuration (default: Release)
REM   -p, --profile     Build with profiling enabled
REM   -t, --test        Run tests after building
REM   -j, --jobs N      Number of parallel jobs (default: auto-detect)
REM   -h, --help        Show this help message

setlocal enabledelayedexpansion

REM Default values
set BUILD_TYPE=Release
set CLEAN_BUILD=false
set RUN_TESTS=false
set PROFILE_BUILD=false
set JOBS=%NUMBER_OF_PROCESSORS%

REM Parse arguments
:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="-c" (
    set CLEAN_BUILD=true
    shift
    goto parse_args
)
if /i "%~1"=="--clean" (
    set CLEAN_BUILD=true
    shift
    goto parse_args
)
if /i "%~1"=="-d" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%~1"=="-p" (
    set BUILD_TYPE=RelWithDebInfo
    set PROFILE_BUILD=true
    shift
    goto parse_args
)
if /i "%~1"=="--profile" (
    set BUILD_TYPE=RelWithDebInfo
    set PROFILE_BUILD=true
    shift
    goto parse_args
)
if /i "%~1"=="-t" (
    set RUN_TESTS=true
    shift
    goto parse_args
)
if /i "%~1"=="--test" (
    set RUN_TESTS=true
    shift
    goto parse_args
)
if /i "%~1"=="-j" (
    set JOBS=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="--jobs" (
    set JOBS=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-h" (
    goto show_help
)
if /i "%~1"=="--help" (
    goto show_help
)
echo Unknown option: %~1
    exit /b 1
)
:end_parse

echo ========================================
echo Lithium Browser Engine Build
echo ========================================
echo Build type: %BUILD_TYPE%
echo Jobs:       %JOBS%
echo.

REM Set build directory based on build type
set BUILD_DIR=build\%BUILD_TYPE%

REM Clean build if requested
if "%CLEAN_BUILD%"=="true" (
    echo Cleaning build directory...
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
)

REM Check if conan is available
where conan >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo Installing dependencies with Conan...
    conan install . --output-folder=%BUILD_DIR% --build=missing -s build_type=%BUILD_TYPE%
    if %ERRORLEVEL% NEQ 0 (
        echo Conan install failed, continuing without it...
    )
) else (
    echo Conan not found, skipping dependency installation...
)

REM Configure
echo Configuring CMake...
if "%PROFILE_BUILD%"=="true" (
    cmake -B %BUILD_DIR% ^
        -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
) else (
    cmake -B %BUILD_DIR% ^
        -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build
echo.
echo Building...
cmake --build %BUILD_DIR% --config %BUILD_TYPE% -j %JOBS%

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================

REM Run tests if requested
if "%RUN_TESTS%"=="true" (
    echo.
    echo Running tests...
    cd %BUILD_DIR%
    ctest -C %BUILD_TYPE% --output-on-failure
    cd ..\..
)

goto end

:show_help
echo Lithium Browser Engine Build Script for Windows
echo Usage: build.bat [options]
echo   -c, --clean       Clean build (remove build directory first)
echo   -d, --debug       Build with Debug configuration (default: Release)
echo   -p, --profile     Build with profiling enabled
echo   -t, --test        Run tests after building
echo   -j, --jobs N      Number of parallel jobs (default: auto-detect)
echo   -h, --help        Show this help message
exit /b 0

:end
endlocal
