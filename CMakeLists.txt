cmake_minimum_required(VERSION 3.16)
project(TimesliceMerger VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

link_directories(/opt/local/lib)

# Find ROOT (required for TTree/TChain approach)
find_package(ROOT REQUIRED)

# Find PODIO (required for data types)
find_package(podio REQUIRED)

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)

# Find yaml-cpp
find_package(yaml-cpp REQUIRED)

# Find HepMC3 (optional, for HepMC3 backend support)
find_package(HepMC3 QUIET)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${ROOT_INCLUDE_DIRS})

# Create the standalone executable
add_executable(timeslice_merger
    src/DataSource.cc
    src/EDM4hepDataSource.cc
    src/OutputHandler.cc
    src/EDM4hepOutputHandler.cc
    src/TimesliceMerger.cc
    src/CommandLineParser.cc
    src/timeslice_merger_main.cc
)

# Conditionally add HepMC3 sources if HepMC3 is found
if(HepMC3_FOUND)
    message(STATUS "HepMC3 found - enabling HepMC3 backend support")
    target_sources(timeslice_merger PRIVATE
        src/HepMC3DataSource.cc
        src/HepMC3OutputHandler.cc
    )
    target_compile_definitions(timeslice_merger PRIVATE HAVE_HEPMC3)
else()
    message(STATUS "HepMC3 not found - HepMC3 backend will not be available")
endif()

# Link against ROOT, PODIO and EDM4HEP
target_link_libraries(timeslice_merger
    ${ROOT_LIBRARIES}
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
    yaml-cpp
)

# Conditionally link HepMC3 if found
if(HepMC3_FOUND)
    target_link_libraries(timeslice_merger HepMC3::HepMC3)
endif()

# Create test executable for file format verification
add_executable(test_file_format
    src/test_file_format.cc
)

# Link against ROOT only for basic format test
target_link_libraries(test_file_format
    ${ROOT_LIBRARIES}
)

# Create Podio API test executable 
add_executable(test_podio_format
    src/test_podio_format.cc
)

# Link against ROOT, PODIO and EDM4HEP for Podio API test
target_link_libraries(test_podio_format
    ${ROOT_LIBRARIES}
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
)

# Add ROOT compilation flags
#target_compile_definitions(timeslice_merger PRIVATE ${ROOT_CXX_FLAGS})

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executables
install(TARGETS timeslice_merger test_file_format test_podio_format DESTINATION bin)