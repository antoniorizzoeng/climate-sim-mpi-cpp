add_executable(bench_exchange bench_exchange.cpp)
target_link_libraries(bench_exchange PRIVATE MPI::MPI_CXX)
target_include_directories(bench_exchange PRIVATE ${CMAKE_SOURCE_DIR}/include)
