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
find_package(PkgConfig REQUIRED)
pkg_check_modules(SPOT REQUIRED IMPORTED_TARGET libspot)

# --- core library ---
add_library(ltlf_ek
  src/variables.cpp
  src/consistency.cpp
  src/output_labeled_transducer.cpp
  src/transducer_io.cpp
  src/ltlf_to_dfa.cpp
  src/solve_dfa.cpp
  src/dfa_product.cpp
)
target_include_directories(ltlf_ek
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ltlf_ek PUBLIC PkgConfig::SPOT)

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
    tests/transducer_io_test.cpp
    tests/ltlf_to_dfa_test.cpp
    tests/dfa_product_test.cpp
  )
  target_link_libraries(unit_tests PRIVATE ltlf_ek GTest::gtest_main)

  include(GoogleTest)
  gtest_discover_tests(unit_tests)
endif()
