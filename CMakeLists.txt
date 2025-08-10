cmake_minimum_required(VERSION 3.16)
project(climate-sim-mpi-cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(yaml-cpp REQUIRED)

option(BUILD_BENCH "Build benchmark targets" ON)

include(CTest)

add_subdirectory(src)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

if (BUILD_BENCH)
  add_subdirectory(bench)
endif()
