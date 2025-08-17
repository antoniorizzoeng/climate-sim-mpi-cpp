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
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(NETCDF QUIET netcdf)
  endif()

  if(NETCDF_FOUND)
    message(STATUS "NetCDF found via pkg-config: enabling NetCDF (IC + snapshots)")
    target_compile_definitions(core PUBLIC HAS_NETCDF=1)
    target_include_directories(core PUBLIC ${NETCDF_INCLUDE_DIRS})
    target_link_libraries(core PUBLIC ${NETCDF_LIBRARIES})
  else()
    find_package(NetCDFCxx4 QUIET)
    find_package(NetCDF QUIET)
    if(NetCDFCxx4_FOUND)
      message(STATUS "NetCDF C++4 found: enabling NetCDF (IC + snapshots)")
      target_compile_definitions(core PUBLIC HAS_NETCDF=1)
      target_link_libraries(core PUBLIC NetCDF::NetCDF_CXX4)
    elseif(NetCDF_FOUND)
      message(STATUS "NetCDF C library found: enabling NetCDF (IC + snapshots)")
      target_compile_definitions(core PUBLIC HAS_NETCDF=1)
      target_link_libraries(core PUBLIC NetCDF::NetCDF)
    else()
      message(WARNING "NetCDF not found: NetCDF features disabled")
    endif()
  endif()
endif()

add_executable(climate_sim main.cpp)
target_link_libraries(climate_sim PRIVATE core)
