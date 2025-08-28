include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(MPI_TEST_PROCS 4 CACHE STRING "MPI ranks used for tests" FORCE)
if(NOT MPIEXEC_EXECUTABLE)
  set(MPIEXEC_EXECUTABLE mpirun)
endif()
if(NOT MPIEXEC_NUMPROC_FLAG)
  set(MPIEXEC_NUMPROC_FLAG -np)
endif()
set(MPIEXEC_PREFLAGS "--oversubscribe")

message(STATUS "MPI_TEST_PROCS = ${MPI_TEST_PROCS}")
message(STATUS "MPIEXEC_EXECUTABLE = ${MPIEXEC_EXECUTABLE}")
message(STATUS "MPIEXEC_NUMPROC_FLAG = ${MPIEXEC_NUMPROC_FLAG}")
message(STATUS "MPIEXEC_PREFLAGS = ${MPIEXEC_PREFLAGS}")

include(CTest)
enable_testing()
include(GoogleTest)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_IT_COMPILE_DEFS
  CLIMATE_SIM_EXE="$<TARGET_FILE:climate_sim>"
  PYTHON_EXECUTABLE="${Python3_EXECUTABLE}"
  SCRIPTS_DIR="${CMAKE_SOURCE_DIR}/scripts"
  MPIEXEC_EXECUTABLE="${MPIEXEC_EXECUTABLE}"
  MPIEXEC_NUMPROC_FLAG="${MPIEXEC_NUMPROC_FLAG}"
  MPIEXEC_PREFLAGS="${MPIEXEC_PREFLAGS}"
  INTEGRATION_MPI_PROCS=${MPI_TEST_PROCS}
)

function(apply_mpi_wrapper target)
  set_property(TARGET ${target} PROPERTY
    CROSSCOMPILING_EMULATOR
      ${MPIEXEC_EXECUTABLE}
      ${MPIEXEC_PREFLAGS}
      ${MPIEXEC_NUMPROC_FLAG} ${MPI_TEST_PROCS}
  )
endfunction()

# ------------------------------
# Unit tests
# ------------------------------

add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_common INTERFACE MPI::MPI_CXX)

add_executable(test_init simulation/unit/test_init.cpp)
target_link_libraries(test_init PRIVATE core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_init DISCOVERY_TIMEOUT 30)

add_executable(test_io simulation/unit/test_io.cpp)
target_link_libraries(test_io PRIVATE core yaml-cpp GTest::gtest GTest::gtest_main)
target_compile_definitions(test_io PRIVATE CONFIGS_DIR="${CMAKE_SOURCE_DIR}/configs")
gtest_discover_tests(test_io DISCOVERY_TIMEOUT 30)

add_executable(test_field simulation/unit/test_field.cpp)
target_link_libraries(test_field PRIVATE test_common core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_field DISCOVERY_TIMEOUT 30)

add_executable(test_diffusion simulation/unit/test_diffusion.cpp)
target_link_libraries(test_diffusion PRIVATE test_common core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_diffusion DISCOVERY_TIMEOUT 30)

add_executable(test_stability simulation/unit/test_stability.cpp)
target_link_libraries(test_stability PRIVATE core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_stability DISCOVERY_TIMEOUT 30)

add_executable(test_boundary simulation/unit/test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_boundary DISCOVERY_TIMEOUT 30)

add_executable(test_decomp_mpi simulation/unit/test_decomp_mpi.cpp)
target_link_libraries(test_decomp_mpi PRIVATE core GTest::gtest GTest::gtest_main MPI::MPI_CXX)
apply_mpi_wrapper(test_decomp_mpi)
gtest_discover_tests(test_decomp_mpi DISCOVERY_TIMEOUT 60)

add_executable(test_halo simulation/unit/test_halo.cpp)
target_link_libraries(test_halo PRIVATE core GTest::gtest GTest::gtest_main MPI::MPI_CXX)
apply_mpi_wrapper(test_halo)
gtest_discover_tests(test_halo DISCOVERY_TIMEOUT 60)

# ------------------------------
# Integration tests
# ------------------------------

add_library(integration_helpers STATIC
  simulation/integration/integration_helpers.cpp
)
target_include_directories(integration_helpers PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/integration
  ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(integration_helpers PUBLIC core MPI::MPI_CXX)
target_compile_definitions(integration_helpers PUBLIC ${_IT_COMPILE_DEFS})

get_target_property(_core_defs core COMPILE_DEFINITIONS)
if(_core_defs)
  list(FIND _core_defs "HAS_NETCDF" _has_nc_idx_helpers)
  if(NOT _has_nc_idx_helpers EQUAL -1)
    target_compile_definitions(integration_helpers PUBLIC HAS_NETCDF=1)
  endif()
endif()

function(add_it name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE integration_helpers GTest::gtest GTest::gtest_main)
  target_compile_definitions(${name} PRIVATE ${_IT_COMPILE_DEFS})
  gtest_discover_tests(${name} DISCOVERY_TIMEOUT 60)
endfunction()

add_it(integration_constant_zero_csv simulation/integration/integration_constant_zero_csv.cpp)
add_it(integration_gaussian_csv      simulation/integration/integration_gaussian_csv.cpp)
add_it(integration_ic_binary         simulation/integration/integration_ic_binary.cpp)
add_it(integration_netcdf_output     simulation/integration/integration_netcdf_output.cpp)
add_it(integration_diffusion         simulation/integration/integration_diffusion.cpp)
add_it(integration_advection         simulation/integration/integration_advection.cpp)
add_it(integration_decomp_coverage   simulation/integration/integration_decomp_coverage.cpp)
add_it(integration_csv_vs_nc         simulation/integration/integration_csv_vs_nc.cpp)
add_it(integration_boundary_error    simulation/integration/integration_boundary_error.cpp)
add_it(integration_ic_netcdf         simulation/integration/integration_ic_netcdf.cpp)

# ------------------------------
# Visualization pytests
# ------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_test(
  NAME Unit_Python
  COMMAND ${Python3_EXECUTABLE} -m pytest tests/visualization
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

set_tests_properties(Unit_Python PROPERTIES
  ENVIRONMENT "PYTHONPATH=${CMAKE_SOURCE_DIR};MPLBACKEND=Agg"
  LABELS "python;visualization"
)