cmake_minimum_required(VERSION 3.12)
project(test_host C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# Find HIDAPI Library
# ------------------------------------------------------------------------------

# Method 1: Try finding HIDAPI using the CMake CONFIG files (standard in modern HIDAPI)
find_package(hidapi CONFIG QUIET)
if(hidapi_FOUND)
    message(STATUS "Found hidapi via CMake CONFIG")
    if(TARGET hidapi::hidapi)
        set(HIDAPI_TARGET hidapi::hidapi)
    elseif(TARGET hidapi::hidraw)
        set(HIDAPI_TARGET hidapi::hidraw)
    elseif(TARGET hidapi::libusb)
        set(HIDAPI_TARGET hidapi::libusb)
    endif()
endif()

# Method 2: Fallback to PkgConfig (common on Linux, macOS, and MSYS2/MinGW Windows)
if(NOT HIDAPI_TARGET)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        # Search for any of the common hidapi packages
        pkg_search_module(HIDAPI REQUIRED IMPORTED_TARGET hidapi hidapi-hidraw hidapi-libusb)
        if(HIDAPI_FOUND)
            message(STATUS "Found hidapi via PkgConfig: ${HIDAPI_LIBRARIES}")
            set(HIDAPI_TARGET PkgConfig::HIDAPI)
        endif()
    endif()
endif()

# Method 3: Last fallback - manual find of headers and libraries
if(NOT HIDAPI_TARGET)
    message(STATUS "Falling back to manual search for hidapi")
    find_path(HIDAPI_INCLUDE_DIR NAMES hidapi.h PATH_SUFFIXES hidapi)
    find_library(HIDAPI_LIBRARY NAMES hidapi hidapi-hidraw hidapi-libusb)
    
    if(HIDAPI_INCLUDE_DIR AND HIDAPI_LIBRARY)
        message(STATUS "Found hidapi library: ${HIDAPI_LIBRARY}")
        message(STATUS "Found hidapi headers: ${HIDAPI_INCLUDE_DIR}")
        
        add_library(hidapi_manual INTERFACE)
        target_include_directories(hidapi_manual INTERFACE ${HIDAPI_INCLUDE_DIR})
        target_link_libraries(hidapi_manual INTERFACE ${HIDAPI_LIBRARY})
        
        # Windows requires linking setupapi when compiling with raw hidapi
        if(WIN32)
            target_link_libraries(hidapi_manual INTERFACE setupapi)
        endif()
        
        set(HIDAPI_TARGET hidapi_manual)
    else()
        message(FATAL_ERROR "Could not find HIDAPI library or headers. Please install HIDAPI.")
    endif()
endif()

# ------------------------------------------------------------------------------
# Build Executable Target
# ------------------------------------------------------------------------------

add_executable(test_host test_host.c)
target_link_libraries(test_host PRIVATE ${HIDAPI_TARGET})
