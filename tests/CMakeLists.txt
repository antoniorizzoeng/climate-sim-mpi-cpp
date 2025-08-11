include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(CTest)
enable_testing()
include(GoogleTest)

add_library(test_common INTERFACE)
target_include_directories(test_common INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_common INTERFACE MPI::MPI_CXX)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE gtest_main core yaml-cpp)
add_test(NAME config_suite COMMAND test_config)

target_compile_definitions(test_config PRIVATE CONFIGS_DIR="${CMAKE_SOURCE_DIR}/configs")

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
target_link_libraries(test_decomp_mpi PRIVATE test_common core GTest::gtest)
add_test(NAME decomp_mpi
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
          $<TARGET_FILE:test_decomp_mpi>)
