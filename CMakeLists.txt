cmake_minimum_required(VERSION 3.16)
project(climate-sim-mpi-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(MPI REQUIRED)
find_package(yaml-cpp REQUIRED)

include(CTest)

option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    message(STATUS "Building with coverage flags")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

add_subdirectory(src)

set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest" FORCE)
set(INSTALL_GMOCK OFF CACHE BOOL "Disable installation of googlemock" FORCE)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

# -----------------------
# Install target + export
# -----------------------
install(TARGETS climate_sim
  EXPORT climate-sim-mpi-cppTargets
  RUNTIME DESTINATION bin
)

install(EXPORT climate-sim-mpi-cppTargets
  FILE climate-sim-mpi-cppTargets.cmake
  NAMESPACE ClimateSim::
  DESTINATION lib/cmake/climate-sim-mpi-cpp
)

# -----------------------
# Package config + version
# -----------------------
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/climate-sim-mpi-cppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfig.cmake"
  INSTALL_DESTINATION lib/cmake/climate-sim-mpi-cpp
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/climate-sim-mpi-cppConfigVersion.cmake"
  DESTINATION lib/cmake/climate-sim-mpi-cpp
)
