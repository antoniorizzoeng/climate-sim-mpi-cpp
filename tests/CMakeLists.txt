include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(NOT DEFINED MPI_TEST_PROCS)
  cmake_host_system_information(RESULT _LOGICAL_CORES QUERY NUMBER_OF_LOGICAL_CORES)
  if(DEFINED ENV{GITHUB_ACTIONS})
    set(_DEFAULT_MPI_PROCS 4)
  else()
    if(_LOGICAL_CORES AND _LOGICAL_CORES GREATER 0)
      if(_LOGICAL_CORES GREATER 4)
        set(_DEFAULT_MPI_PROCS 4)
      else()
        set(_DEFAULT_MPI_PROCS ${_LOGICAL_CORES})
      endif()
    else()
      set(_DEFAULT_MPI_PROCS 4)
    endif()
  endif()
  set(MPI_TEST_PROCS ${_DEFAULT_MPI_PROCS} CACHE STRING "MPI ranks used for tests")
endif()

message(STATUS "MPI_TEST_PROCS = ${MPI_TEST_PROCS}")

if(DEFINED ENV{GITHUB_ACTIONS})
  set(MPIEXEC_PREFLAGS "--oversubscribe")
endif()

include(CTest)
enable_testing()
include(GoogleTest)

# Common interface for tests that just need headers and MPI
add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_common INTERFACE MPI::MPI_CXX)

# ------------------------------
# Unit tests
# ------------------------------

add_executable(test_init test_init.cpp)
target_link_libraries(test_init PRIVATE gtest_main core)
add_test(NAME init_suite COMMAND test_init)

add_executable(test_io test_io.cpp)
target_link_libraries(test_io PRIVATE gtest_main core yaml-cpp)
add_test(NAME io_suite COMMAND test_io)
target_compile_definitions(test_io PRIVATE CONFIGS_DIR="${CMAKE_SOURCE_DIR}/configs")

add_executable(test_halo test_halo.cpp)
target_link_libraries(test_halo PRIVATE core gtest_main MPI::MPI_CXX)
add_test(NAME halo_mpi
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_PREFLAGS} ${MPIEXEC_NUMPROC_FLAG} ${MPI_TEST_PROCS}
          $<TARGET_FILE:test_halo>)

add_executable(test_field test_field.cpp)
target_link_libraries(test_field PRIVATE test_common core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_field)

add_executable(test_diffusion test_diffusion.cpp)
target_link_libraries(test_diffusion PRIVATE test_common core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_diffusion)

add_executable(test_stability test_stability.cpp)
target_link_libraries(test_stability PRIVATE core gtest_main)
add_test(NAME stability COMMAND test_stability)

add_executable(test_decomp_mpi test_decomp_mpi.cpp)
target_link_libraries(test_decomp_mpi PRIVATE core gtest_main MPI::MPI_CXX)
add_test(NAME decomp_mpi
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_PREFLAGS} ${MPIEXEC_NUMPROC_FLAG} ${MPI_TEST_PROCS}
          $<TARGET_FILE:test_decomp_mpi>)

add_executable(test_boundary test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE core gtest_main MPI::MPI_CXX)
add_test(NAME boundary_single_rank COMMAND $<TARGET_FILE:test_boundary>)

# ------------------------------
# Integration tests (full binary + scripts)
# ------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_executable(test_integration test_integration.cpp)
target_include_directories(test_integration PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_integration PRIVATE core GTest::gtest GTest::gtest_main)

# Pass paths/flags into the test
# (climate_sim target is defined in src/CMakeLists.txt)
target_compile_definitions(test_integration PRIVATE
  CLIMATE_SIM_EXE="$<TARGET_FILE:climate_sim>"
  PYTHON_EXECUTABLE="${Python3_EXECUTABLE}"
  SCRIPTS_DIR="${CMAKE_SOURCE_DIR}/scripts"
)

# Ensure mpiexec variables are available; provide sane fallbacks if missing
if(NOT MPIEXEC_EXECUTABLE)
  set(MPIEXEC_EXECUTABLE mpirun)
endif()
if(NOT MPIEXEC_NUMPROC_FLAG)
  set(MPIEXEC_NUMPROC_FLAG -np)
endif()
target_compile_definitions(test_integration PRIVATE
  MPIEXEC_EXECUTABLE="${MPIEXEC_EXECUTABLE}"
  MPIEXEC_NUMPROC_FLAG="${MPIEXEC_NUMPROC_FLAG}"
)

# Propagate NetCDF capability flag (HAS_NETCDF) from core to the test if present
get_target_property(_core_defs core COMPILE_DEFINITIONS)
if(_core_defs)
  list(FIND _core_defs "HAS_NETCDF" _has_nc_idx)
  if(NOT _has_nc_idx EQUAL -1)
    target_compile_definitions(test_integration PRIVATE HAS_NETCDF=1)
  endif()
endif()

# Discover & register integration tests
gtest_discover_tests(test_integration DISCOVERY_TIMEOUT 30)
