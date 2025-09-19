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

find_package(NetCDF QUIET)

if(NetCDF_FOUND)
    message(STATUS "Found NetCDF via CMake config")
    target_link_libraries(core PUBLIC NetCDF::NetCDF)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NETCDF REQUIRED netcdf)
    message(STATUS "Found NetCDF via pkg-config")
    target_include_directories(core PUBLIC ${NETCDF_INCLUDE_DIRS})
    target_link_libraries(core PUBLIC ${NETCDF_LIBRARIES})
endif()

add_executable(climate_sim main.cpp)
target_link_libraries(climate_sim PRIVATE core)
