cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add common library search paths for EIC container
link_directories(/opt/local/lib /usr/local/lib)

# Print diagnostic information
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

# Add potential EIC package locations to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH
  /usr/local
  /opt/detector
  /opt/software
  /opt/local
)

# Find PODIO (required for standalone merger)
find_package(podio REQUIRED)
if(podio_FOUND)
  message(STATUS "Found podio: ${podio_DIR}")
else()
  message(FATAL_ERROR "podio not found. Ensure the EIC environment is properly set up.")
endif()

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)  
if(EDM4HEP_FOUND)
  message(STATUS "Found EDM4HEP: ${EDM4HEP_DIR}")
else()
  message(FATAL_ERROR "EDM4HEP not found. Ensure the EIC environment is properly set up.")
endif()

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)
if(yaml-cpp_FOUND)
  message(STATUS "Found yaml-cpp: ${yaml-cpp_DIR}")
else()
  message(FATAL_ERROR "yaml-cpp not found. Install yaml-cpp or ensure it's available in the environment.")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Enable testing
enable_testing()

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

# Add basic test to verify build environment
add_executable(basic_test tests/basic_test.cpp)
add_test(NAME BasicTest COMMAND basic_test)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executable
install(TARGETS timeslice_merger DESTINATION bin)