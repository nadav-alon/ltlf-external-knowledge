cmake_minimum_required(VERSION 3.16)
project(ltlf_ek LANGUAGES CXX)

# LTLf synthesis with external information (see main.tex).
# Thin domain wrappers over Spot; custom types for the non-standard
# lambda-split transducers and NFAs that have no native Spot equivalent.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# --- Spot (via pkg-config: libspot) ---
# Spot >= 2.15 is REQUIRED, not preferred: before 2.15,
# mtdfa_winning_strategy(backprop_nodes=true) segfaults on a product MTDFA
# (upstream bug, Spot issue #639, fixed by e4452735e).  src/solve_mtdfa.cpp
# depends on that call, so an older Spot does not fail a test --- it crashes.
#
# SPOT_ROOT points at a Spot install to prefer over the system one; it also
# pins the RUNTIME loader (below), which matters because a stray
# LD_LIBRARY_PATH can otherwise shadow the library this build was compiled
# against and silently pair 2.15 headers with an older .so.
set(SPOT_ROOT "" CACHE PATH "Root of the Spot install to build and run against")
if(SPOT_ROOT)
  set(ENV{PKG_CONFIG_PATH} "${SPOT_ROOT}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SPOT REQUIRED IMPORTED_TARGET libspot>=2.15)

if(SPOT_ROOT)
  # DT_RPATH, not DT_RUNPATH: only DT_RPATH takes precedence over
  # LD_LIBRARY_PATH, so this build cannot be hijacked by another Spot install.
  add_link_options(-Wl,--disable-new-dtags)
  set(CMAKE_BUILD_RPATH "${SPOT_ROOT}/lib")
  set(CMAKE_INSTALL_RPATH "${SPOT_ROOT}/lib")
endif()

# --- core library ---
add_library(ltlf_ek
  src/variables.cpp
  src/consistency.cpp
  src/output_labeled_transducer.cpp
  src/transducer_io.cpp
  src/role.cpp
  src/ltlf_to_dfa.cpp
  src/solve_dfa.cpp
  src/product.cpp
  src/dfa_product.cpp
  src/emits_dfa.cpp
  src/turn_order.cpp
  src/solve_mtdfa.cpp
  src/mtdfa_product.cpp
  src/synthesis.cpp
  src/verify_controller.cpp
  src/cli.cpp
  src/bench.cpp
)
target_include_directories(ltlf_ek
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ltlf_ek PUBLIC PkgConfig::SPOT)

# --- ltlf-ek-synth: CLI front end (docs/prd/cli-wrapper.md) ---
add_executable(ltlf-ek-synth src/ltlf_ek_synth.cpp)
target_link_libraries(ltlf-ek-synth PRIVATE ltlf_ek)

# --- ltlfsynt (Spot's own CLI synthesizer) ---
# Located for tests/ltlfsynt_oracle_test.cpp's external realizability oracle
# (docs/prd/ltlfsynt-oracle.md).  Optional: a clean box without Spot's CLI
# tools still builds and tests green (that suite GTEST_SKIP()s instead).
# SPOT_ROOT first: the oracle must come from the same Spot as the linked
# library, or it silently compares against a different version's semantics.
find_program(LTLFSYNT_EXECUTABLE ltlfsynt
  HINTS ${SPOT_ROOT}/bin)

# --- tests ---
option(LTLF_EK_BUILD_TESTS "Build unit tests" ON)
if(LTLF_EK_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(unit_tests
    tests/variables_test.cpp
    tests/consistency_test.cpp
    tests/output_labeled_transducer_test.cpp
    tests/symbolic_transducer_contract_test.cpp
    tests/transducer_io_test.cpp
    tests/ltlf_to_dfa_test.cpp
    tests/product_test.cpp
    tests/product_build_equivalence_test.cpp
    tests/dfa_product_test.cpp
    tests/emits_dfa_test.cpp
    tests/turn_order_test.cpp
    tests/mtdfa_product_test.cpp
    tests/verify_controller_test.cpp
    tests/cli_test.cpp
    tests/ltlf_ek_synth_test.cpp
    tests/ltlfsynt_oracle_test.cpp
    tests/bench_test.cpp
  )
  target_link_libraries(unit_tests PRIVATE ltlf_ek GTest::gtest_main)
  # tests/ltlf_ek_synth_test.cpp drives the built ltlf-ek-synth binary as a
  # subprocess (docs/prd/cli-wrapper.md "Test oracles"); it needs the binary
  # built first and its path baked in.
  add_dependencies(unit_tests ltlf-ek-synth)
  target_compile_definitions(unit_tests PRIVATE
    "LTLF_EK_SYNTH_BINARY=\"$<TARGET_FILE:ltlf-ek-synth>\"")
  # tests/ltlfsynt_oracle_test.cpp drives Spot's own ltlfsynt binary as a
  # subprocess (docs/prd/ltlfsynt-oracle.md); LTLFSYNT_BIN (runtime env)
  # overrides this compile-time path, and the suite GTEST_SKIP()s when
  # neither resolves to a runnable binary.
  if(LTLFSYNT_EXECUTABLE)
    target_compile_definitions(unit_tests PRIVATE
      "LTLFSYNT_BINARY=\"${LTLFSYNT_EXECUTABLE}\"")
  endif()

  include(GoogleTest)
  gtest_discover_tests(unit_tests)
endif()
