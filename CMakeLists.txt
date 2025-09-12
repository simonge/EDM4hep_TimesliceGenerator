cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ROOT - required for RDataFrame approach
find_package(ROOT QUIET COMPONENTS Core RIO Tree TreePlayer RDataFrame)

if(ROOT_FOUND)
    message(STATUS "ROOT found: Using ROOT ${ROOT_VERSION}")
    set(USE_ROOT TRUE)
    add_compile_definitions(USE_ROOT)
else()
    message(WARNING "ROOT not found - building minimal version without ROOT DataFrames")
    set(USE_ROOT FALSE)
endif()

# Try to find yaml-cpp, but make it optional
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(YAMLCPP yaml-cpp)
endif()

if(NOT PkgConfig_FOUND OR NOT YAMLCPP_FOUND)
    find_package(yaml-cpp QUIET)
    if(yaml-cpp_FOUND)
        set(YAMLCPP_FOUND TRUE)
        set(YAMLCPP_LIBRARIES yaml-cpp)
        set(YAMLCPP_INCLUDE_DIRS "")
    endif()
endif()

if(YAMLCPP_FOUND)
    message(STATUS "yaml-cpp found: Using YAML configuration support")
    add_compile_definitions(USE_YAML_CPP)
else()
    message(WARNING "yaml-cpp not found - building without YAML configuration support")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(USE_ROOT)
    include_directories(${ROOT_INCLUDE_DIRS})
endif()

if(YAMLCPP_FOUND AND YAMLCPP_INCLUDE_DIRS)
    include_directories(${YAMLCPP_INCLUDE_DIRS})
endif()

# Create the standalone executable
add_executable(timeslice_merger
    src/StandaloneTimesliceMerger.cc
    src/timeslice_merger_main.cc
)

# Link libraries
target_link_libraries(timeslice_merger)

if(USE_ROOT)
    target_link_libraries(timeslice_merger ${ROOT_LIBRARIES})
    target_compile_definitions(timeslice_merger PRIVATE ${ROOT_CXX_FLAGS})
endif()

if(YAMLCPP_FOUND)
    target_link_libraries(timeslice_merger ${YAMLCPP_LIBRARIES})
endif()

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executable
install(TARGETS timeslice_merger DESTINATION bin)

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  ROOT support: ${USE_ROOT}")
message(STATUS "  YAML-cpp support: ${YAMLCPP_FOUND}")
if(USE_ROOT)
    message(STATUS "  ROOT version: ${ROOT_VERSION}")
    message(STATUS "  ROOT libraries: ${ROOT_LIBRARIES}")
endif()
if(YAMLCPP_FOUND)
    message(STATUS "  yaml-cpp libraries: ${YAMLCPP_LIBRARIES}")
endif()