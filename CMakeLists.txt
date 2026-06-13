cmake_minimum_required(VERSION 3.20)
project(accu_coupled_oscillator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Header-only library: the modular physics components and core ODE machinery.
add_library(sopot_core INTERFACE)
target_include_directories(sopot_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# The two-point-mass coupled oscillator demo.
add_executable(oscillator_demo examples/oscillator_demo.cpp)
target_link_libraries(oscillator_demo PRIVATE sopot_core)

# Run the demo from the build dir so oscillator_data.csv lands there, e.g.:
#   cmake -B build && cmake --build build
#   cd build && ./oscillator_demo && python ../scripts/make_plot.py
