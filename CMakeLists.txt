cmake_minimum_required(VERSION 3.1)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "mpsl")
  project(mpsl C CXX)
endif()

# =============================================================================
# [MPSL - Configuration]
# =============================================================================

set(MPSL_DIR         "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "Location of 'mpsl'")
set(ASMJIT_DIR       "${MPSL_DIR}/../asmjit"     CACHE PATH "Location of 'asmjit'")

set(MPSL_EMBED       FALSE         CACHE BOOLEAN "Embed 'mpsl' library (no targets)")
set(MPSL_STATIC      ${MPSL_EMBED} CACHE BOOLEAN "Build 'mpsl' library as static")
set(MPSL_BUILD_TEST  FALSE         CACHE BOOLEAN "Build 'mpsl_test' applications")

if(NOT DEFINED ASMJIT_EMBED)
  set(ASMJIT_EMBED TRUE)
endif()

# =============================================================================
# [MPSL - Project]
# =============================================================================

include("${MPSL_DIR}/CxxProject.cmake")
cxx_project(mpsl)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  list(APPEND MPSL_PRIVATE_CFLAGS /GF)
  list(APPEND MPSL_PRIVATE_CFLAGS_DBG /GS /GR-)
  list(APPEND MPSL_PRIVATE_CFLAGS_REL /Oi /Oy /GS- /GR-)

  # Enable multi-process compilation.
  if(NOT MSVC60 AND NOT MSVC70 AND NOT MSVC71)
    list(APPEND MPSL_PRIVATE_CFLAGS /MP)
  endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)$")
  cxx_detect_standard(MPSL_PRIVATE_CFLAGS)
  cxx_detect_cflags(MPSL_PRIVATE_CFLAGS
    "-fvisibility=hidden"
    "-fno-exceptions"
    "-fno-tree-vectorize"
    "-Winconsistent-missing-override")
  list(APPEND MPSL_PRIVATE_CFLAGS_REL -fmerge-all-constants)
endif()

# Use Unicode by default on Windows target.
if(WIN32)
  list(APPEND MPSL_PRIVATE_CFLAGS "${CXX_DEFINE}_UNICODE")
endif()

include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND MPSL_DEPS ${ASMJIT_LIBS})
list(APPEND MPSL_PRIVATE_CFLAGS ${ASMJIT_CFLAGS})

set(MPSL_LIBS ${MPSL_DEPS})
if(NOT MPSL_EMBED)
  list(INSERT MPSL_LIBS 0 mpsl)
endif()

list(REMOVE_DUPLICATES MPSL_DEPS)
list(REMOVE_DUPLICATES MPSL_PRIVATE_CFLAGS)

cxx_project_info(mpsl)

# =============================================================================
# [MPSL - Source]
# =============================================================================

cxx_add_source(mpsl MPSL_SRC mpsl
  mpsl_apibegin.h
  mpsl_apiend.h
  mpsl_build.h

  mpsl.cpp
  mpsl.h
  mpsl_p.h

  mpast.cpp
  mpast_p.h
  mpastoptimizer.cpp
  mpastoptimizer_p.h
  mpatomic_p.h
  mpcodegen.cpp
  mpcodegen_p.h
  mpfold.cpp
  mpfold_p.h
  mpformatutils.cpp
  mpformatutils_p.h
  mphash.cpp
  mphash_p.h
  mpir.cpp
  mpir_p.h
  mpirpass.cpp
  mpirpass_p.h
  mpirtox86.cpp
  mpirtox86_p.h
  mplang.cpp
  mplang_p.h
  mpmath.cpp
  mpmath_p.h
  mpparser.cpp
  mpparser_p.h
  mpstrtod_p.h
  mptokenizer.cpp
  mptokenizer_p.h
)

# =============================================================================
# [MPSL - Targets]
# =============================================================================

if(NOT MPSL_EMBED)
  cxx_add_library(mpsl mpsl
    "${MPSL_SRC};${ASMJIT_SRC}"
    "${MPSL_DEPS}"
    "${MPSL_PRIVATE_CFLAGS}"
    "${MPSL_PRIVATE_CFLAGS_REL}"
    "${MPSL_PRIVATE_CFLAGS_DBG}")
  target_include_directories(mpsl PRIVATE BEFORE ${ASMJIT_INCLUDE_DIR})

  foreach(i ${MPSL_SRC})
    get_filename_component(path ${i} PATH)
    get_filename_component(name ${i} NAME)
    string(REGEX REPLACE "^${MPSL_SOURCE_DIR}/" "" targetpath "${path}")
    if(${name} MATCHES ".h$")
      if(NOT "${name}" MATCHES "_p.h$")
        install(FILES ${i} DESTINATION "include/${targetpath}")
      endif()
    endif()
  endforeach()
endif()

# =============================================================================
# [MPSL - ...]
# =============================================================================

if(MPSL_BUILD_TEST)
  set(MPSL_SRC_TEST mp_dsp mp_test mp_tutorial)

  foreach(file ${MPSL_SRC_TEST})
    cxx_add_executable(mpsl ${file}
      "test/${file}.cpp;test/mp_utils.h"
      "${MPSL_LIBS}"
      "${MPSL_CFLAGS}"
      ""
      "")
  endforeach(file)
endif()
