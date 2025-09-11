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
message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# Add potential EIC package locations to CMAKE_PREFIX_PATH
list(APPEND CMAKE_PREFIX_PATH
  /usr/local
  /opt/detector
  /opt/software
  /opt/local
  $ENV{CONDA_PREFIX}
  $ENV{VIRTUAL_ENV}
)

# Also search in common library directories
set(CMAKE_LIBRARY_PATH
  ${CMAKE_LIBRARY_PATH}
  /usr/local/lib
  /usr/local/lib64  
  /opt/detector/lib
  /opt/software/lib
)

set(CMAKE_INCLUDE_PATH
  ${CMAKE_INCLUDE_PATH}
  /usr/local/include
  /opt/detector/include
  /opt/software/include
)

# Find PODIO (required for standalone merger)
message(STATUS "Searching for podio package...")
find_package(podio QUIET)
if(podio_FOUND)
  message(STATUS "Found podio: ${podio_DIR}")
else()
  message(STATUS "podio not found. Searching in additional locations...")
  # Try to find podio manually
  find_path(PODIO_INCLUDE_DIR
    NAMES podio/Frame.h
    PATHS /usr/local/include /opt/detector/include /opt/software/include
    NO_DEFAULT_PATH
  )
  find_library(PODIO_LIBRARY
    NAMES podio podioRootIO
    PATHS /usr/local/lib /usr/local/lib64 /opt/detector/lib /opt/software/lib
    NO_DEFAULT_PATH
  )
  
  if(PODIO_INCLUDE_DIR AND PODIO_LIBRARY)
    message(STATUS "Found podio manually: includes=${PODIO_INCLUDE_DIR}, lib=${PODIO_LIBRARY}")
    # Create imported target manually
    add_library(podio::podioRootIO UNKNOWN IMPORTED)
    set_target_properties(podio::podioRootIO PROPERTIES
      IMPORTED_LOCATION "${PODIO_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${PODIO_INCLUDE_DIR}"
    )
  else()
    message(FATAL_ERROR "podio not found. Ensure the EIC environment is properly set up.
    Checked paths:
    - Include: /usr/local/include, /opt/detector/include, /opt/software/include
    - Library: /usr/local/lib, /usr/local/lib64, /opt/detector/lib, /opt/software/lib")
  endif()
endif()

# Find EDM4HEP
message(STATUS "Searching for EDM4HEP package...")
find_package(EDM4HEP QUIET)  
if(EDM4HEP_FOUND)
  message(STATUS "Found EDM4HEP: ${EDM4HEP_DIR}")
else()
  message(STATUS "EDM4HEP not found. Searching in additional locations...")
  # Try to find EDM4HEP manually
  find_path(EDM4HEP_INCLUDE_DIR
    NAMES edm4hep/MCParticleCollection.h
    PATHS /usr/local/include /opt/detector/include /opt/software/include
    NO_DEFAULT_PATH
  )
  find_library(EDM4HEP_LIBRARY
    NAMES edm4hep
    PATHS /usr/local/lib /usr/local/lib64 /opt/detector/lib /opt/software/lib
    NO_DEFAULT_PATH
  )
  
  if(EDM4HEP_INCLUDE_DIR AND EDM4HEP_LIBRARY)
    message(STATUS "Found EDM4HEP manually: includes=${EDM4HEP_INCLUDE_DIR}, lib=${EDM4HEP_LIBRARY}")
    # Create imported targets manually
    add_library(EDM4HEP::edm4hep UNKNOWN IMPORTED)
    set_target_properties(EDM4HEP::edm4hep PROPERTIES
      IMPORTED_LOCATION "${EDM4HEP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${EDM4HEP_INCLUDE_DIR}"
    )
    # Also create the Dict target (might not exist but won't hurt)
    add_library(EDM4HEP::edm4hepDict UNKNOWN IMPORTED)
    set_target_properties(EDM4HEP::edm4hepDict PROPERTIES
      IMPORTED_LOCATION "${EDM4HEP_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${EDM4HEP_INCLUDE_DIR}"
    )
  else()
    message(FATAL_ERROR "EDM4HEP not found. Ensure the EIC environment is properly set up.
    Checked paths:
    - Include: /usr/local/include, /opt/detector/include, /opt/software/include  
    - Library: /usr/local/lib, /usr/local/lib64, /opt/detector/lib, /opt/software/lib")
  endif()
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