# ########################################################################
# Copyright (C) 2016-2022 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
# ies of the Software, and to permit persons to whom the Software is furnished
# to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
# PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
# CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# ########################################################################

# ########################################################################
# A helper function to prefix a source list of files with a common path into a new list (non-destructive)
# ########################################################################
function( prepend_path prefix source_list_of_files return_list_of_files )
  foreach( file ${${source_list_of_files}} )
    if(IS_ABSOLUTE ${file} )
      list( APPEND new_list ${file} )
    else( )
      list( APPEND new_list ${prefix}/${file} )
    endif( )
  endforeach( )
  set( ${return_list_of_files} ${new_list} PARENT_SCOPE )
endfunction( )


if (USE_ONEAPI)
find_program(DPCPP_EXECUTABLE NAMES dpcpp
	HINTS $ENV{CMPLR_ROOT}/linux/bin $ENV{ONEAPI_ROOT}/compiler/latest/linux/bin
	PATHS /opt/intel/oneapi/compiler/latest/linux/bin)

if(DPCPP_EXECUTABLE)
	get_filename_component(DPCPP_CORE_BINDIR ${DPCPP_EXECUTABLE} DIRECTORY)
	get_filename_component(DPCPP_CORE_LIBDIR "${DPCPP_CORE_BINDIR}/../lib" ABSOLUTE)
  get_filename_component(DPCPP_INCLUDE_DIR "${DPCPP_CORE_BINDIR}/../include" ABSOLUTE)
endif()

message(STATUS "DPCPP_CORE_BINDIR: ${DPCPP_CORE_BINDIR}")
message(STATUS "DPCPP_CORE_LIBDIR: ${DPCPP_CORE_LIBDIR}")
message(STATUS "DPCPP_INCLUDE_DIR: ${DPCPP_INCLUDE_DIR}")

# the ENABLE_OMP_OFFLOAD is only required to unhide MKL::sycl in intel's MKLConfig.cmake file
# set(ENABLE_OMP_OFFLOAD ON)
set(MKL_THREADING sequential)

find_package(MKL CONFIG
	HINTS $ENV{MKLROOT}/lib/cmake/mkl  $ENV{ONEAPI_ROOT}/mkl/latest/lib/cmake/mkl
	PATHS /opt/intel/oneapi/mkl/latest/lib/cmake/mkl)

message(STATUS "DPCPP : ${DPCPP_EXECUTABLE} MKL: ${MKL_FOUND} ")

if(DPCPP_EXECUTABLE AND MKL_FOUND)
  message(STATUS "Found both MLK and DPCPP")
  #include(mkl_and_icpx)
endif()
endif()


# ########################################################################
# Main
# ########################################################################
prepend_path( ".." hipblas_headers_public relative_hipblas_headers_public )
if( USE_ONEAPI)
  add_subdirectory(oneApi_detail)
  set( hipblas_source "${CMAKE_CURRENT_SOURCE_DIR}/oneApi_detail/hipblas.cpp" )  
elseif( NOT USE_CUDA )
  set( hipblas_source "${CMAKE_CURRENT_SOURCE_DIR}/amd_detail/hipblas.cpp" )
else( )
  set( hipblas_source "${CMAKE_CURRENT_SOURCE_DIR}/nvidia_detail/hipblas.cpp" )
endif( )

set (hipblas_f90_source
  hipblas_module.f90
)

# Create hipBLAS Fortran module
#if(NOT WIN32)
#    add_library(hipblas_fortran ${hipblas_f90_source})
#endif()

if(BUILD_ADDRESS_SANITIZER)
    add_link_options(-fuse-ld=lld)
endif()

add_library( hipblas
  ${hipblas_source}
  ${CMAKE_CURRENT_SOURCE_DIR}/hipblas_auxiliary.cpp
  ${relative_hipblas_headers_public}
)
add_library( roc::hipblas ALIAS hipblas )

# External header includes included as system files
target_include_directories( hipblas
  SYSTEM PRIVATE
    $<BUILD_INTERFACE:${HIP_INCLUDE_DIRS}>
)

# Build hipblas from source on AMD platform
if( NOT USE_CUDA )
  if( NOT TARGET rocblas )
    if( CUSTOM_ROCBLAS )
      set ( ENV{rocblas_DIR} ${CUSTOM_ROCBLAS})
      find_package( rocblas REQUIRED CONFIG NO_CMAKE_PATH )
    elseif( WIN32 )
        find_package( rocblas REQUIRED CONFIG PATHS ${ROCBLAS_PATH})
    else()
    message("should look for oneMKL ??")
    #  find_package( rocblas REQUIRED CONFIG PATHS /opt/rocm /opt/rocm/rocblas )
    endif( )
  endif( )

  # do we need hip::host
  #target_link_libraries( hipblas PRIVATE roc::rocblas hip::host )

  # Add rocSOLVER as a dependency if BUILD_WITH_SOLVER is on
  if( BUILD_WITH_SOLVER )
    if( NOT TARGET rocsolver )
      if( CUSTOM_ROCSOLVER)
        set ( ENV{rocsolver_DIR} ${CUSTOM_ROCSOLVER})
        find_package( rocsolver REQUIRED CONFIG NO_CMAKE_PATH )
      elseif(WIN32)
        find_package( rocsolver REQUIRED CONFIG PATHS ${ROCSOLVER_PATH} )
      else()
        find_package( rocsolver REQUIRED CONFIG PATHS /opt/rocm /opt/rocm/rocsolver /usr/local/rocsolver )
      endif()
    endif( )
    target_link_libraries( hipblas PRIVATE roc::rocsolver )
  endif( )

  if( CUSTOM_TARGET )
    target_link_libraries( hipblas PRIVATE hip::${CUSTOM_TARGET} )
  endif( )

else( )
  target_compile_definitions( hipblas PRIVATE __HIP_PLATFORM_NVCC__ )

  target_link_libraries( hipblas PRIVATE ${CUDA_CUBLAS_LIBRARIES} )

  # External header includes included as system files
  target_include_directories( hipblas
    SYSTEM PRIVATE
      $<BUILD_INTERFACE:${CUDA_INCLUDE_DIRS}>
  )
endif( )

if (USE_ONEAPI)
  target_compile_definitions( hipblas PRIVATE __HIP_PLATFORM_SPIRV__ )
  target_link_libraries(hipblas
  sycl_wrapper
#  mkl_sycl
#  mkl_intel_ilp64
#  mkl_sequential
#  mkl_core
#  sycl
#  OpenCL
)
endif()

# Internal header includes
target_include_directories( hipblas
  PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/library/include>
          $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/hipblas>
          $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/include
          ${CMAKE_CURRENT_SOURCE_DIR}
)

#rocm_set_soversion( hipblas ${hipblas_SOVERSION} )
set_target_properties( hipblas PROPERTIES CXX_EXTENSIONS NO )
set_target_properties( hipblas PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/staging" )
set_target_properties( hipblas PROPERTIES DEBUG_POSTFIX "-d" )

if (WIN32)
  add_custom_command( TARGET hipblas POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/staging/$<TARGET_FILE_NAME:hipblas> ${PROJECT_BINARY_DIR}/clients/staging/$<TARGET_FILE_NAME:hipblas> )
  if( ${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_custom_command( TARGET hipblas POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/staging/hipblas-d.pdb ${PROJECT_BINARY_DIR}/clients/staging/hipblas-d.pdb )
  endif()
endif()

# Package that helps me set visibility for function names exported from shared library
include( GenerateExportHeader )
set_target_properties( hipblas PROPERTIES CXX_VISIBILITY_PRESET "hidden" VISIBILITY_INLINES_HIDDEN ON )
generate_export_header( hipblas EXPORT_FILE_NAME ${PROJECT_BINARY_DIR}/include/hipblas/hipblas-export.h )

#if (BUILD_FILE_REORG_BACKWARD_COMPATIBILITY AND NOT WIN32)
#  rocm_wrap_header_file(
#    hipblas-version.h hipblas-export.h
#    GUARDS SYMLINK WRAPPER
#    WRAPPER_LOCATIONS ${CMAKE_INSTALL_INCLUDEDIR} hipblas/${CMAKE_INSTALL_INCLUDEDIR}
#  )
#endif( )


# Following Boost conventions of prefixing 'lib' on static built libraries, across all platforms
if( NOT BUILD_SHARED_LIBS )
  set_target_properties( hipblas PROPERTIES PREFIX "lib" )
endif( )

############################################################
# Installation

#rocm_install_targets(
#  TARGETS hipblas
#  INCLUDE
#    ${CMAKE_BINARY_DIR}/include
#)
#         PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ

#if ( NOT USE_CUDA )
#    rocm_export_targets(
#        TARGETS roc::hipblas
#	DEPENDS PACKAGE hip
#	NAMESPACE roc::
#    )
#else( )
#    rocm_export_targets(
#        TARGETS roc::hipblas
#	DEPENDS PACKAGE HIP
#	NAMESPACE roc::
#    )
#endif( )

# Force installation of .f90 module files
#rocm_install(FILES "hipblas_module.f90"
#        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/hipblas")

#if(BUILD_FILE_REORG_BACKWARD_COMPATIBILITY AND NOT WIN32)
#  rocm_install(
#    DIRECTORY
#       "${PROJECT_BINARY_DIR}/hipblas"
#        DESTINATION "." )

#  if ( NOT WIN32 )

#    #Create SymLink for Fortran Object Module for backward compatibility
#    rocm_install(
#      CODE "
#        set(PREFIX \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX})
#	set(INPUT_FILE \${PREFIX}/include/hipblas/hipblas_module.f90)
#        set(SYMLINK_LOCATIONS \${PREFIX}/hipblas/include \${PREFIX}/include)
#        foreach(LOCATION IN LISTS SYMLINK_LOCATIONS)
#          file(MAKE_DIRECTORY \${LOCATION})
#          execute_process(COMMAND ln -sfr \${INPUT_FILE} \${LOCATION})
#          message(STATUS \"Created symlink in \${LOCATION} to \${INPUT_FILE}.\")
#        endforeach()
#        "
#    )
#  endif() #NOT WIN32
#  message( STATUS "Backward Compatible Sym Link Created for include directories" )
#endif()

# copy header files
  file( GLOB header_files ${PROJECT_BINARY_DIR}/include/hipblas/*.h)
  foreach( file_i ${header_files})
    message(STATUS " Sarbojit :::: ${file_i}")
    file(COPY ${file_i} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/hipblas/")
  endforeach( file_i)

#install hipblas
install(TARGETS hipblas DESTINATION ${CMAKE_INSTALL_LIBDIR})
