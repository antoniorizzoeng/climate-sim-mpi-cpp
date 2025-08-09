add_library(core
    field.cpp decomp.cpp diffusion.cpp advection.cpp boundary.cpp io.cpp)
target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC MPI::MPI_CXX)

add_executable(climate_sim main.cpp)
target_link_libraries(climate_sim PRIVATE core)
