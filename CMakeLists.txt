cmake_minimum_required(VERSION 3.1)

# =============================================================================
# [MPSL - Configuration]
# =============================================================================

# Embedding mode, mpsl will not create any targets (default FALSE).
# set(MPSL_EMBED FALSE)

# Whether to build a static library (default FALSE).
# set(MPSL_STATIC FALSE)

# Whether to build tests and samples (default FALSE).
# set(MPSL_BUILD_TEST FALSE)

# =============================================================================
# [MPSL - Build / Embed]
# =============================================================================

# Do not create a project if this CMakeLists.txt is included from another
# project. This makes it easy to embed or create a static library.
if(NOT CMAKE_PROJECT_NAME OR "${CMAKE_PROJECT_NAME}" MATCHES "^mpsl$")
  project(mpsl C CXX)
  set(MPSL_SIGNATURE "Standalone")
else()
  set(MPSL_SIGNATURE "Included")
endif()

if(MPSL_EMBED)
  set(MPSL_SIGNATURE "${MPSL_SIGNATURE} | Mode=Embed")
  set(MPSL_STATIC TRUE) # Implies MPSL_STATIC.
elseif(MPSL_STATIC)
  set(MPSL_SIGNATURE "${MPSL_SIGNATURE} | Mode=Static")
else()
  set(MPSL_SIGNATURE "${MPSL_SIGNATURE} | Mode=Shared")
endif()

if(MPSL_BUILD_TEST)
  set(MPSL_SIGNATURE "${MPSL_SIGNATURE} | Test=On")
else()
  set(MPSL_SIGNATURE "${MPSL_SIGNATURE} | Test=Off")
endif()

if(NOT MPSL_DIR)
  set(MPSL_DIR ${CMAKE_CURRENT_LIST_DIR})
endif()

message("-- [mpsl] ${MPSL_SIGNATURE}")
message("-- [mpsl] MPSL_DIR=${MPSL_DIR}")

# =============================================================================
# [MPSL - Flags / Deps]
# =============================================================================

set(MPSL_SOURCE_DIR "${MPSL_DIR}/src")          # MPSL source directory.
set(MPSL_INCLUDE_DIR "${MPSL_SOURCE_DIR}")      # MPSL include directory.

set(MPSL_DEPS)                                  # MPSL dependencies (list of libraries) for the linker.
set(MPSL_LIBS)                                  # MPSL dependencies with mpsl included, for consumers.

# Internal, never use.
set(MPSL_D "-D")                                # Used to define a C/C++ preprocessor parameter (-D or /D).
set(MPSL_PRIVATE_CFLAGS)                        # Compiler flags independent of build type.
set(MPSL_PRIVATE_LFLAGS)                        # Linker flags independent of build type.
set(MPSL_PRIVATE_CFLAGS_REL)                    # Compiler flags used only by debug build.
set(MPSL_PRIVATE_CFLAGS_DBG)                    # Compiler flags used only by release build.

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(MSPL_D "/D")
  set(MPSL_PRIVATE_LFLAGS "/OPT:REF /OPT:ICF")

  list(APPEND MPSL_PRIVATE_CFLAGS /GF)
  list(APPEND MPSL_PRIVATE_CFLAGS_DBG /GS /GR-)
  list(APPEND MPSL_PRIVATE_CFLAGS_REL /Oi /Oy /GS- /GR-)

  if(NOT MSVC60 AND NOT MSVC70 AND NOT MSVC71)
    list(APPEND MPSL_PRIVATE_CFLAGS /MP)        # Enable multi-process compilation.
  endif()
endif()

if("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(GNU|Clang)$")
  list(APPEND MPSL_PRIVATE_CFLAGS -fno-exceptions)
endif()

if(MPSL_EMBED)
  list(APPEND MPSL_PRIVATE_CFLAGS "${MPSL_D}MPSL_EMBED")
elseif(MPSL_STATIC)
  list(APPEND MPSL_PRIVATE_CFLAGS "${MPSL_D}MPSL_STATIC")
endif()

# Dependencies.
if(NOT ASMJIT_DIR)
  set(ASMJIT_DIR "${MPSL_DIR}/../asmjit")
endif()
set(ASMJIT_EMBED TRUE)
include("${ASMJIT_DIR}/CMakeLists.txt")
list(APPEND MPSL_DEPS ${ASMJIT_LIBS})
list(APPEND MPSL_PRIVATE_CFLAGS "${MPSL_D}ASMJIT_EMBED")

# Use Unicode by default on Windows target.
if(WIN32)
  list(APPEND MPSL_PRIVATE_CFLAGS "${MPSL_D}_UNICODE")
endif()

set(MPSL_LIBS ${ASMJIT_DEPS})
if(NOT MPSL_EMBED)
  list(INSERT MPSL_LIBS 0 mpsl)
endif()

message("-- [mpsl] MPSL_DEPS=${MPSL_DEPS}")
message("-- [mpsl] MPSL_LIBS=${MPSL_LIBS}")

# =============================================================================
# [MPSL - Macros]
# =============================================================================

macro(mpsl_add_source _out_dst _src_dir)
  set(_src_path "${MPSL_SOURCE_DIR}/${_src_dir}")
  set(_src_list)

  foreach(_arg ${ARGN})
    set(_src_file "${_src_path}/${_arg}")
    list(APPEND _src_list ${_src_file})
  endforeach()

  list(APPEND "${_out_dst}" ${_src_list})
  source_group(${_src_dir} FILES ${_src_list})
endmacro()

macro(mpsl_add_library _target _src _deps _cflags _cflags_dbg _cflags_rel)
  if(NOT MPSL_STATIC)
    add_library(${_target} SHARED ${_src})
  else()
    add_library(${_target} STATIC ${_src})
  endif()

  target_link_libraries(${_target} ${_deps})
  set_target_properties(${_target} PROPERTIES LINK_FLAGS "${MPSL_PRIVATE_LFLAGS}")

  if(CMAKE_BUILD_TYPE)
    if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
      target_compile_options(${_target} PRIVATE ${_cflags} ${_cflags_dbg})
    else()
      target_compile_options(${_target} PRIVATE ${_cflags} ${_cflags_rel})
    endif()
  else()
    target_compile_options(${_target} PRIVATE ${_cflags}
      $<$<CONFIG:Debug>:${_cflags_dbg}>
      $<$<NOT:$<CONFIG:Debug>>:${_cflags_rel}>)
  endif()

  if(NOT MPSL_STATIC)
    install(TARGETS ${_target} DESTINATION "lib${LIB_SUFFIX}")
  endif()
endmacro()

# =============================================================================
# [MPSL - Sources]
# =============================================================================

set(MPSL_SRC "")

mpsl_add_source(MPSL_SRC mpsl
  mpsl_apibegin.h
  mpsl_apiend.h
  mpsl_build.h

  mpsl.cpp
  mpsl.h
  mpsl_p.h

  mpallocator.cpp
  mpallocator_p.h
  mpast.cpp
  mpast_p.h
  mpastanalysis.cpp
  mpastanalysis_p.h
  mpastoptimizer.cpp
  mpastoptimizer_p.h
  mpasttoir.cpp
  mpasttoir_p.h
  mpatomic_p.h
  mpcompiler_x86.cpp
  mpcompiler_x86_p.h
  mpmath.cpp
  mpmath_p.h
  mphash.cpp
  mphash_p.h
  mpir.cpp
  mpir_p.h
  mpparser.cpp
  mpparser_p.h
  mpstrtod_p.h
  mptokenizer.cpp
  mptokenizer_p.h
  mputils.cpp
  mputils_p.h
)

# =============================================================================
# [MPSL - Targets]
# =============================================================================

if(NOT MPSL_EMBED)
  mpsl_add_library(mpsl
    "${MPSL_SRC};${ASMJIT_SRC}"
    "${MPSL_DEPS}"
    "${MPSL_PRIVATE_CFLAGS}"
    "${MPSL_PRIVATE_CFLAGS_REL}"
    "${MPSL_PRIVATE_CFLAGS_DBG}"
  )
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
  set(MPSL_SRC_TEST mp_test mp_tutorial)

  foreach(file ${MPSL_SRC_TEST})
    add_executable(${file} src/app/${file}.cpp src/app/mp_utils.h)
    target_link_libraries(${file} ${MPSL_LIBS})
  endforeach(file)
endif()
