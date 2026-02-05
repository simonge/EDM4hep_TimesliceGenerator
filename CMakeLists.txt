cmake_minimum_required(VERSION 3.16)
project(TimeframeBuilder VERSION 1.0.0)

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
add_executable(timeframe_builder
    src/DataSource.cc
    src/EDM4hepDataSource.cc
    src/DataHandler.cc
    src/EDM4hepDataHandler.cc
    src/TimeframeBuilder.cc
    src/CommandLineParser.cc
    src/timeframe_builder_main.cc
)

# Conditionally add HepMC3 sources if HepMC3 is found
if(HepMC3_FOUND)
    message(STATUS "HepMC3 found - enabling HepMC3 backend support")
    target_sources(timeframe_builder PRIVATE
        src/HepMC3DataSource.cc
        src/HepMC3DataHandler.cc
    )
    target_compile_definitions(timeframe_builder PRIVATE HAVE_HEPMC3)
else()
    message(STATUS "HepMC3 not found - HepMC3 backend will not be available")
endif()

# Link against ROOT, PODIO and EDM4HEP
target_link_libraries(timeframe_builder
    ${ROOT_LIBRARIES}
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
    yaml-cpp
)

# Conditionally link HepMC3 if found
if(HepMC3_FOUND)
    target_link_libraries(timeframe_builder HepMC3::HepMC3)
    # WriterRootTree lives in the optional rootIO component
    if(TARGET HepMC3::rootIO)
        target_link_libraries(timeframe_builder HepMC3::rootIO)
    endif()
endif()

# Add ROOT compilation flags
#target_compile_definitions(timeframe_builder PRIVATE ${ROOT_CXX_FLAGS})

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Install the executables
install(TARGETS timeframe_builder DESTINATION bin)

# Option to build JANA2 plugin
option(BUILD_JANA_PLUGIN "Build JANA2 plugin for TimeframeBuilder" ON)

if(BUILD_JANA_PLUGIN)
    add_subdirectory(jana_plugin)
endif()