include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

if(DEFINED ENV{GITHUB_ACTIONS})
  set(MPI_TEST_PROCS 2)
else()
  set(MPI_TEST_PROCS 4)
endif()

include(CTest)
enable_testing()
include(GoogleTest)

add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_common INTERFACE MPI::MPI_CXX)

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
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${MPI_TEST_PROCS}
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
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${MPI_TEST_PROCS}
          $<TARGET_FILE:test_decomp_mpi>)

add_executable(test_boundary test_boundary.cpp)
target_link_libraries(test_boundary PRIVATE core gtest_main MPI::MPI_CXX)
add_test(NAME boundary_single_rank COMMAND $<TARGET_FILE:test_boundary>)
