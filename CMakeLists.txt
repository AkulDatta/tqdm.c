cmake_minimum_required(VERSION 3.16)

project(tqdm C)

# Require C99 across all targets
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ========================
# Compiler warnings & opts
# ========================
if (MSVC)
  # Treat warnings as errors and enable multi‐processor compilation
  add_compile_options(/W4 /WX /MP)
else()
  add_compile_options(
    -g            # Debug symbols
    -O2           # Optimisation level comparable to Release with debug info
    -Wall -Wextra # Standard warnings
    -Werror       # Fail on warnings
    -Wformat=2    # Stricter printf/scanf checks
    -Wno-missing-field-initializers
    -fPIC         # Position independent code for static lib
  )
  # Whole-program optimisation for GCC
  if (CMAKE_COMPILER_IS_GNUCC)
    add_compile_options(-fwhole-program)
  endif()
endif()

# =============
# Source layout
# =============
set(TQDM_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(TQDM_SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src)

file(GLOB TQDM_SOURCES CONFIGURE_DEPENDS ${TQDM_SRC_DIR}/*.c)
list(REMOVE_ITEM TQDM_SOURCES ${TQDM_SRC_DIR}/main.c) # exclude CLI entry point

# ==========
# Libraries
# ==========
add_library(tqdmlib STATIC ${TQDM_SOURCES})

# Public headers for consumers
target_include_directories(tqdmlib PUBLIC ${TQDM_INCLUDE_DIR})

# ======
# Binaries
# ======
add_executable(tqdm ${TQDM_SRC_DIR}/main.c)
target_link_libraries(tqdm PRIVATE tqdmlib)

# =====
# Tests
# =====
add_executable(test_tqdm   test/test-example.c)
add_executable(test_macros test/test-macros.c)

target_link_libraries(test_tqdm   PRIVATE tqdmlib)
target_link_libraries(test_macros PRIVATE tqdmlib)

# =========
# Unit test integration
# =========
enable_testing()

# Register test binaries with CTest so they can be invoked via `ctest`.
add_test(NAME unit_core    COMMAND test_tqdm)
add_test(NAME unit_macros  COMMAND test_macros)

# Keep quick feedback during normal builds
foreach(test_target IN ITEMS test_tqdm test_macros)
  add_custom_command(TARGET ${test_target}
                     POST_BUILD
                     COMMAND $<TARGET_FILE:${test_target}>
                     COMMENT "Running ${test_target} after build")
endforeach()

# ==============
# clang-format (optional, developer convenience)
# ==============
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
  add_custom_target(cfmt
    COMMAND ${CLANG_FORMAT_EXE} -i -style=file
            ${TQDM_SOURCES}
            ${TQDM_SRC_DIR}/main.c
            test/test-example.c
            test/test-macros.c
    COMMENT "Formatting source files with clang-format")
endif()
