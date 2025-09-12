cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find ROOT - required for RDataFrame approach
find_package(ROOT REQUIRED COMPONENTS Core RIO Tree TreePlayer RDataFrame)

# Find yaml-cpp - required for configuration
find_package(yaml-cpp REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${ROOT_INCLUDE_DIRS})

# Create the standalone executable
add_executable(timeslice_merger
    src/StandaloneTimesliceMerger.cc
    src/timeslice_merger_main.cc
)

# Link against ROOT and yaml-cpp
target_link_libraries(timeslice_merger
    ${ROOT_LIBRARIES}
    yaml-cpp
)

# Set ROOT compile definitions
target_compile_definitions(timeslice_merger PRIVATE ${ROOT_CXX_FLAGS})

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executable
install(TARGETS timeslice_merger DESTINATION bin)

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  ROOT version: ${ROOT_VERSION}")
message(STATUS "  ROOT libraries: ${ROOT_LIBRARIES}")
message(STATUS "  Using required ROOT and yaml-cpp dependencies")