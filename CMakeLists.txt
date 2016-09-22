cmake_minimum_required(VERSION 3.1)

# =============================================================================
# [MPSL - Configuration]
# =============================================================================

# set(MPSL_EMBED FALSE)                         # Embed (no targets, just project).
# set(MPSL_STATIC FALSE)                        # Whether to build a static library.
# set(MPSL_BUILD_TEST FALSE)                    # Whether to build tests and samples.

# =============================================================================
# [MPSL - Project]
# =============================================================================

if(NOT MPSL_DIR)
  set(MPSL_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

include("${MPSL_DIR}/CxxProject.cmake")
cxx_project(mpsl)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(MPSL_PRIVATE_LFLAGS "/OPT:REF /OPT:ICF")

  list(APPEND MPSL_PRIVATE_CFLAGS /GF)
  list(APPEND MPSL_PRIVATE_CFLAGS_DBG /GS /GR-)
  list(APPEND MPSL_PRIVATE_CFLAGS_REL /Oi /Oy /GS- /GR-)

  if(NOT MSVC60 AND NOT MSVC70 AND NOT MSVC71)
    list(APPEND MPSL_PRIVATE_CFLAGS /MP)        # Enable multi-process compilation.
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

if(NOT ASMJIT_DIR)
  set(ASMJIT_DIR "${MPSL_DIR}/../asmjit")
endif()
set(ASMJIT_EMBED TRUE)
include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND MPSL_DEPS ${ASMJIT_LIBS})
list(APPEND MPSL_PRIVATE_CFLAGS "${ASMJIT_CFLAGS}")

# Use Unicode by default on Windows target.
if(WIN32)
  list(APPEND MPSL_PRIVATE_CFLAGS "${CXX_DEFINE}_UNICODE")
endif()

set(MPSL_LIBS ${ASMJIT_DEPS})
if(NOT MPSL_EMBED)
  list(INSERT MPSL_LIBS 0 mpsl)
endif()

cxx_project_info(mpsl)

# =============================================================================
# [MPSL - Sources]
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
  mpastanalysis.cpp
  mpastanalysis_p.h
  mpastoptimizer.cpp
  mpastoptimizer_p.h
  mpasttoir.cpp
  mpasttoir_p.h
  mpatomic_p.h
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
