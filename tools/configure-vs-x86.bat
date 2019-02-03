@echo off

set MPSL_CURRENT_DIR=%CD%
set MPSL_BUILD_DIR="build_vs_x86"

mkdir ..\%MPSL_BUILD_DIR%
cd ..\%MPSL_BUILD_DIR%
cmake .. -G"Visual Studio 16" -A Win32 -DMPSL_TEST=1
cd %MPSL_CURRENT_DIR%
