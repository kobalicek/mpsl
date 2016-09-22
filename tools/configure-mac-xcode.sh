#!/bin/sh

MPSL_CURRENT_DIR=`pwd`
MPSL_BUILD_DIR="build_xcode"
MPSL_ASMJIT_DIR="../asmjit"

mkdir ../${MPSL_BUILD_DIR}
cd ../${MPSL_BUILD_DIR}
cmake .. -G"Xcode" -DASMJIT_DIR="${MPSL_ASMJIT_DIR}" -DMPSL_BUILD_TEST=1
cd ${MPSL_CURRENT_DIR}
