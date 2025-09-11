cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

link_directories(/opt/local/lib)

# Find PODIO (required for standalone merger)
find_package(podio REQUIRED)

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the standalone executable
add_executable(timeslice_merger
    src/StandaloneTimesliceMerger.cc
    src/MutableRootReader.cc
    src/timeslice_merger_main.cc
)

# Link against PODIO and EDM4HEP
target_link_libraries(timeslice_merger
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
    yaml-cpp
)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executable
install(TARGETS timeslice_merger DESTINATION bin)