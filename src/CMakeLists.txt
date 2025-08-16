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

# --- NetCDF support ---
option(ENABLE_NETCDF "Enable NetCDF I/O" ON)

if(ENABLE_NETCDF)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(NETCDF netcdf)

  if(NETCDF_FOUND)
    message(STATUS "NetCDF found via pkg-config: enabling NetCDF IC reader")
    target_compile_definitions(core PRIVATE HAS_NETCDF=1)
    target_include_directories(core PRIVATE ${NETCDF_INCLUDE_DIRS})
    target_link_libraries(core PRIVATE ${NETCDF_LIBRARIES})
  else()
    message(WARNING "NetCDF not found: NetCDF IC support disabled")
  endif()
endif()

add_executable(climate_sim main.cpp)
target_link_libraries(climate_sim PRIVATE core)
