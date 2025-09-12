cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the standalone executable
add_executable(timeslice_merger
    src/StandaloneTimesliceMerger.cc
    src/timeslice_merger_main.cc
)

# Link against yaml-cpp
target_link_libraries(timeslice_merger
    yaml-cpp
)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executable
install(TARGETS timeslice_merger DESTINATION bin)