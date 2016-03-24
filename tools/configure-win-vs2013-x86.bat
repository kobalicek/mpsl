@echo off

set MPSL_CURRENT_DIR=%CD%
set MPSL_BUILD_DIR="build_vs2013_x86"
set MPSL_ASMJIT_DIR="../asmjit"

mkdir ..\%MPSL_BUILD_DIR%
cd ..\%MPSL_BUILD_DIR%
cmake .. -G"Visual Studio 12" -DASMJIT_DIR="%MPSL_ASMJIT_DIR%" -DMPSL_BUILD_TEST=1
cd %MPSL_CURRENT_DIR%
