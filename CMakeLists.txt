cmake_minimum_required(VERSION 3.10)
project(hdf5_vols)

#-----------------------------------------------------------------------------
# Define Options
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# Compiler Optimization
#-----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("IN DEBUG MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    add_compile_definitions(HERMES_LOG_VERBOSITY=10)
else()
    message("IN RELEASE MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
    add_compile_definitions(HERMES_LOG_VERBOSITY=1)
endif()
add_compile_options(-march=native -fomit-frame-pointer)

#-----------------------------------------------------------------------------
# Find Packages
#-----------------------------------------------------------------------------

# HermesShm
find_package(HermesShm CONFIG REQUIRED)
message(STATUS "found hermes_shm.h at ${HermesShm_INCLUDE_DIRS}")

# YAML-CPP
find_package(yaml-cpp REQUIRED)
message(STATUS "found yaml-cpp at ${yaml-cpp_DIR}")

# Catch2
find_package(Catch2 3.0.1 REQUIRED)
message(STATUS "found catch2.h at ${Catch2_CXX_INCLUDE_DIRS}")

# MPI
find_package(MPI REQUIRED COMPONENTS C CXX)
message(STATUS "found mpi.h at ${MPI_CXX_INCLUDE_DIRS}")

# OpenMP
if(BUILD_OpenMP_TESTS)
    find_package(OpenMP REQUIRED COMPONENTS C CXX)
    message(STATUS "found omp.h at ${OpenMP_CXX_INCLUDE_DIRS}")
endif()

# Cereal
find_package(cereal REQUIRED)
if(cereal)
    message(STATUS "found cereal")
endif()

# Pkg-Config
find_package(PkgConfig REQUIRED)
if(PkgConfig)
    message(STATUS "found pkg config")
endif()

# LIBAIO
find_library(LIBAIO_LIBRARY NAMES aio)
if(LIBAIO_LIBRARY)
    message(STATUS "found libaio at ${LIBAIO_LIBRARY}")
else()
    set(LIBAIO_LIBRARY aio)
    message(STATUS "Assuming it was installed with our aio spack")
endif()

# HDF5
set(HERMES_REQUIRED_HDF5_VERSION 1.14.0)
set(HERMES_REQUIRED_HDF5_COMPONENTS C)
find_package(HDF5 ${HERMES_REQUIRED_HDF5_VERSION} CONFIG NAMES hdf5
        COMPONENTS ${HERMES_REQUIRED_HDF5_COMPONENTS} shared)
if(HDF5_FOUND)
    message(STATUS "found HDF5 ${HDF5_VERSION} at ${HDF5_INCLUDE_DIR}")
    set(HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES
            ${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES}
            ${HDF5_INCLUDE_DIR})
    set(HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES
            ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES}
            ${HDF5_C_SHARED_LIBRARY})
else()
    # Allow for HDF5 autotools builds
    find_package(HDF5 ${HERMES_REQUIRED_HDF5_VERSION} MODULE REQUIRED
            COMPONENTS ${HERMES_REQUIRED_HDF5_COMPONENTS})
    if(HDF5_FOUND)
        set(HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES
                ${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES}
                ${HDF5_INCLUDE_DIRS})
        set(HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES
                ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES}
                ${HDF5_LIBRARIES})
    else()
        message(FATAL_ERROR "Could not find HDF5, please set HDF5_DIR (1.13.0) or HDF5_ROOT (1.13.1).")
    endif()
endif()


#------------------------------------------------------------------------------
# Setup CMake Environment
#------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
        ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries.")

#------------------------------------------------------------------------------
# Setup install and output Directories
#------------------------------------------------------------------------------
if(NOT HERMES_INSTALL_BIN_DIR)
    set(HERMES_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()
if(NOT HERMES_INSTALL_LIB_DIR)
    set(HERMES_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()
if(NOT HERMES_INSTALL_INCLUDE_DIR)
    set(HERMES_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()
if(NOT HERMES_INSTALL_DATA_DIR)
    set(HERMES_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()

#-----------------------------------------------------------------------------
# Build Main Packages
#-----------------------------------------------------------------------------
add_library(replicate_vol SHARED H5VLreplicate_vol.cc)
include_directories(${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES})
target_link_libraries(replicate_vol
        MPI::MPI_CXX
        ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES})
message("${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES} ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES} ${HDF5_DEFINITIONS}")

add_library(compress_vol SHARED H5VLcompress_vol.cc)
include_directories(${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES})
target_link_libraries(compress_vol
        MPI::MPI_CXX
        ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES})
message("${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES} ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES} ${HDF5_DEFINITIONS}")

add_library(pfs_vol SHARED H5VLpfs_vol.cc)
include_directories(${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES})
target_link_libraries(pfs_vol
        MPI::MPI_CXX
        ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES})
message("${HDF5_HERMES_VFD_EXT_INCLUDE_DEPENDENCIES} ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES} ${HDF5_DEFINITIONS}")

add_executable(hermes_vol_main main.cc)
target_link_libraries(hermes_vol_main
        MPI::MPI_CXX ${HDF5_HERMES_VFD_EXT_LIB_DEPENDENCIES})