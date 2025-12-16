cmake_minimum_required(VERSION 3.12)
project(heinzinger_control_py LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# --- Your Project's Source Files ---
set(SOURCES
    bindings.cpp
    Heinzinger.cpp
    ProjectGlobals.cpp # <--- ADD THIS NEW SOURCE FILE
)

# --- Include Directories for Your Project & Dependencies ---
include_directories(
    ${Python_INCLUDE_DIRS}
    ${pybind11_INCLUDE_DIRS}
    "${CMAKE_CURRENT_SOURCE_DIR}/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/pstreams"
)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    message(STATUS "macOS detected. Adding Homebrew default include path: /opt/homebrew/include")
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

find_package(PkgConfig QUIET)
set(LIBUSB_1_FOUND_BY_PKGCONFIG FALSE)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBUSB_1_PKGCONFIG libusb-1.0)
    if(LIBUSB_1_PKGCONFIG_FOUND)
        set(LIBUSB_1_FOUND_BY_PKGCONFIG TRUE)
        message(STATUS "Found libusb-1.0 via PkgConfig.")
        message(STATUS "  PkgConfig Include Dirs: ${LIBUSB_1_PKGCONFIG_INCLUDE_DIRS}")
        message(STATUS "  PkgConfig Library Dirs: ${LIBUSB_1_PKGCONFIG_LIBRARY_DIRS}")
        message(STATUS "  PkgConfig Libraries: ${LIBUSB_1_PKGCONFIG_LIBRARIES}")
        if(NOT (CMAKE_SYSTEM_NAME MATCHES "Darwin" AND ("${LIBUSB_1_PKGCONFIG_INCLUDE_DIRS}" STREQUAL "/opt/homebrew/include" OR "${LIBUSB_1_PKGCONFIG_INCLUDE_DIRS}" STREQUAL "/opt/homebrew/Cellar/libusb/1.0.28/include/libusb-1.0")))
             include_directories(${LIBUSB_1_PKGCONFIG_INCLUDE_DIRS})
        endif()
    else()
        message(WARNING "libusb-1.0 not found via PkgConfig, though PkgConfig tool was found.")
    endif()
else()
    message(WARNING "PkgConfig tool not found. Will rely on system paths or direct linking for libusb-1.0.")
endif()

pybind11_add_module(heinzinger_control ${SOURCES})

# Add this definition to guard main() in Heinzinger.cpp
target_compile_definitions(heinzinger_control PRIVATE PYBIND11_MODULE_BUILD)

if(LIBUSB_1_FOUND_BY_PKGCONFIG)
    target_link_libraries(heinzinger_control PRIVATE ${LIBUSB_1_PKGCONFIG_LIBRARIES})
else()
    message(WARNING "Linking libusb-1.0 directly as 'usb-1.0' (common on macOS/Homebrew) or 'libusb-1.0'.")
    if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        target_link_libraries(heinzinger_control PRIVATE usb-1.0)
    else()
        target_link_libraries(heinzinger_control PRIVATE libusb-1.0)
    endif()
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux|Darwin")
    find_package(Threads REQUIRED)
    target_link_libraries(heinzinger_control PRIVATE Threads::Threads)
endif()