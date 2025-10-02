add_library(core
    init.cpp
    field.cpp
    decomp.cpp
    diffusion.cpp
    advection.cpp
    boundary.cpp
    io.cpp
    halo.cpp
)

target_include_directories(core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(core PUBLIC MPI::MPI_CXX yaml-cpp)

find_path(PNETCDF_INCLUDE_DIR pnetcdf.h
          HINTS /usr/include /usr/local/include)

find_library(PNETCDF_LIBRARY pnetcdf
             HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib)

if(NOT PNETCDF_INCLUDE_DIR OR NOT PNETCDF_LIBRARY)
    message(FATAL_ERROR "PnetCDF not found. Install libpnetcdf-dev or build from source.")
endif()

message(STATUS "Found PnetCDF: include at ${PNETCDF_INCLUDE_DIR}, lib at ${PNETCDF_LIBRARY}")

target_include_directories(core PUBLIC ${PNETCDF_INCLUDE_DIR})
target_link_libraries(core PUBLIC ${PNETCDF_LIBRARY})

add_executable(climate_sim main.cpp)
target_link_libraries(climate_sim PRIVATE core)
