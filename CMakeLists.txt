cmake_minimum_required(VERSION 3.8)

# Don't create a project if it was already created by another CMakeLists.txt.
# This allows one library to embed another library without a project collision.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" STREQUAL "mpsl")
  project(mpsl CXX)
endif()

# =============================================================================
# [MPSL - Configuration]
# =============================================================================

if (NOT DEFINED MPSL_DIR)
  set(MPSL_DIR "${CMAKE_CURRENT_LIST_DIR}")
endif()

if (NOT DEFINED ASMJIT_DIR)
  set(ASMJIT_DIR "${MPSL_DIR}/../asmjit")
endif()

set(MPSL_DIR    "${MPSL_DIR}"   CACHE PATH "Location of 'mpsl'")
set(ASMJIT_DIR  "${ASMJIT_DIR}" CACHE PATH "Location of 'asmjit'")

set(MPSL_TEST   FALSE           CACHE BOOL "Build 'mpsl' test applications")
set(MPSL_EMBED  FALSE           CACHE BOOL "Embed 'mpsl' library (no targets)")
set(MPSL_STATIC ${MPSL_EMBED}   CACHE BOOL "Build 'mpsl' library as static")

# =============================================================================
# [MPSL - Project]
# =============================================================================

set(MPSL_INCLUDE_DIRS ${MPSL_DIR}/src)
set(MPSL_DEPS "")
set(MPSL_LIBS "")
set(MPSL_CFLAGS "")
set(MPSL_PRIVATE_CFLAGS "")
set(MPSL_PRIVATE_CFLAGS_DBG "")
set(MPSL_PRIVATE_CFLAGS_REL "")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC" OR "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  set(MPSL_PRIVATE_LFLAGS "/OPT:REF /OPT:ICF")

  list(APPEND MPSL_PRIVATE_CFLAGS -MP)        # [+] Multi-process compilation
  list(APPEND MPSL_PRIVATE_CFLAGS -GF)        # [+] Eliminate duplicate strings.
  list(APPEND MPSL_PRIVATE_CFLAGS -GR-)       # [-] Runtime type information.
  list(APPEND MPSL_PRIVATE_CFLAGS_DBG -GS)    # [+] Buffer security-check.
  list(APPEND MPSL_PRIVATE_CFLAGS_REL -GS-)   # [-] Buffer security-check.
  list(APPEND MPSL_PRIVATE_CFLAGS_REL -O2)    # [+] Favor speed over size.
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang|AppleClang)$")
  list(APPEND MPSL_PRIVATE_CFLAGS -Wall -Wextra)
endif()

if(NOT DEFINED ASMJIT_EMBED)
  set(ASMJIT_EMBED TRUE)
endif()
include("${ASMJIT_DIR}/CMakeLists.txt")

list(APPEND MPSL_DEPS ${ASMJIT_LIBS})
list(APPEND MPSL_PRIVATE_CFLAGS "${ASMJIT_CFLAGS}")

list(APPEND MPSL_LIBS ${MPSL_DEPS})
if (NOT MPSL_EMBED)
  list(INSERT MPSL_LIBS 0 mpsl)
endif()

if (MPSL_EMBED)
  set(MPSL_TARGET_TYPE "EMBED")
elseif (MPSL_STATIC)
  set(MPSL_TARGET_TYPE "STATIC")
else()
  set(MPSL_TARGET_TYPE "SHARED")
endif()

if (MPSL_EMBED OR MPSL_STATIC)
  list(APPEND MPSL_CFLAGS -DMPSL_STATIC)
endif()

list(REMOVE_DUPLICATES MPSL_DEPS)
list(REMOVE_DUPLICATES MPSL_PRIVATE_CFLAGS)

# =============================================================================
# [MPSL - Source]
# =============================================================================

set(MPSL_SRC_LIST
  mpsl/mpsl_apibegin.h
  mpsl/mpsl_apiend.h
  mpsl/mpsl_build.h

  mpsl/mpsl.cpp
  mpsl/mpsl.h
  mpsl/mpsl_p.h

  mpsl/mpast.cpp
  mpsl/mpast_p.h
  mpsl/mpastoptimizer.cpp
  mpsl/mpastoptimizer_p.h
  mpsl/mpatomic_p.h
  mpsl/mpcodegen.cpp
  mpsl/mpcodegen_p.h
  mpsl/mpfold.cpp
  mpsl/mpfold_p.h
  mpsl/mpformatutils.cpp
  mpsl/mpformatutils_p.h
  mpsl/mphash.cpp
  mpsl/mphash_p.h
  mpsl/mpir.cpp
  mpsl/mpir_p.h
  mpsl/mpirpass.cpp
  mpsl/mpirpass_p.h
  mpsl/mpirtox86.cpp
  mpsl/mpirtox86_p.h
  mpsl/mplang.cpp
  mpsl/mplang_p.h
  mpsl/mpmath.cpp
  mpsl/mpmath_p.h
  mpsl/mpparser.cpp
  mpsl/mpparser_p.h
  mpsl/mpstrtod_p.h
  mpsl/mptokenizer.cpp
  mpsl/mptokenizer_p.h
)

set(MPSL_SRC)
foreach(_src_file ${MPSL_SRC_LIST})
  list(APPEND MPSL_SRC src/${_src_file})
endforeach()
source_group(TREE "${MPSL_DIR}" FILES ${MPSL_SRC})

# =============================================================================
# [MPSL - Targets]
# =============================================================================

if (NOT MPSL_EMBED)
  add_library(mpsl ${MPSL_TARGET_TYPE} ${MPSL_SRC} ${ASMJIT_SRC})
  target_link_libraries(mpsl PRIVATE ${MPSL_DEPS})
  target_compile_options(mpsl PUBLIC ${MPSL_CFLAGS})
  target_compile_options(mpsl PRIVATE ${MPSL_PRIVATE_CFLAGS}
    $<$<CONFIG:Debug>:${MPSL_PRIVATE_CFLAGS_DBG}>
    $<$<NOT:$<CONFIG:Debug>>:${MPSL_PRIVATE_CFLAGS_REL}>)
  target_compile_features(mpsl PUBLIC cxx_std_11)
  set_property(TARGET mpsl PROPERTY CXX_VISIBILITY_PRESET hidden)

  target_include_directories(mpsl PRIVATE BEFORE ${ASMJIT_INCLUDE_DIRS})
  target_include_directories(mpsl PUBLIC BEFORE ${MPSL_INCLUDE_DIRS})

  foreach(_src_file ${MPSL_SRC_LIST})
    if ("${_src_file}" MATCHES "\\.h$" AND NOT "${_src_file}" MATCHES "_p\\.h$")
      get_filename_component(_src_dir ${_src_file} PATH)
      install(FILES "${MPSL_DIR}/src/${_src_file}" DESTINATION "include/${_src_dir}")
    endif()
  endforeach()
endif()

if (MPSL_TEST)
  foreach(_target mp_dsp mp_test mp_tutorial)
    add_executable(${_target} test/${_target}.cpp)
    target_link_libraries(${_target} ${MPSL_LIBS})
    target_compile_options(${_target} PRIVATE ${MPSL_PRIVATE_CFLAGS}
      $<$<CONFIG:Debug>:${MPSL_PRIVATE_CFLAGS_DBG}>
      $<$<NOT:$<CONFIG:Debug>>:${MPSL_PRIVATE_CFLAGS_REL}>)
    target_compile_features(${_target} PUBLIC cxx_std_11)
    set_property(TARGET ${_target} PROPERTY CXX_VISIBILITY_PRESET hidden)
  endforeach()
endif()
