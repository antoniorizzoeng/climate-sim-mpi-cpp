add_library(core
    init.cpp
    step.cpp
    field.cpp
    decomp.cpp
    diffusion.cpp
    advection.cpp
    boundary.cpp
    io.cpp
    halo.cpp
)

option(ENABLE_NATIVE_OPTIMIZATIONS "Optimize the simulation kernels for this CPU" OFF)
if(ENABLE_NATIVE_OPTIMIZATIONS)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=native" HAS_MARCH_NATIVE)
    if(NOT HAS_MARCH_NATIVE)
        message(FATAL_ERROR "ENABLE_NATIVE_OPTIMIZATIONS requires -march=native support")
    endif()
    target_compile_options(core PRIVATE -O3 -march=native)
endif()

option(ENABLE_OPENMP "Allow OpenMP threads within each MPI rank" OFF)
if(ENABLE_OPENMP)
    find_package(OpenMP REQUIRED COMPONENTS CXX)
    target_link_libraries(core PRIVATE OpenMP::OpenMP_CXX)
endif()

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
