cmake_minimum_required(VERSION 3.16)
project(EDM4HEPTimesliceExample VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JANA
find_package(JANA REQUIRED)

# Find PODIO (required for EDM4HEPTimesliceExample)
find_package(podio REQUIRED)

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the plugin
add_jana_plugin(EDM4HEPTimesliceExample
    SOURCES
        src/EDM4HEPTimesliceExample.cc
        src/CollectionTabulatorsEDM4HEP.cc
    PUBLIC_HEADER
        include/CollectionTabulatorsEDM4HEP.h
        include/MyFileReaderEDM4HEP.h
        include/MyFileReaderGeneratorEDM4HEP.h
        include/MyFileWriterEDM4HEP.h
        include/MyTimesliceBuilderEDM4HEP.h
        include/SimTrackerHitCollector_factory.h
)

# Link against PODIO and EDM4HEP
target_link_libraries(EDM4HEPTimesliceExample PUBLIC 
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Add tests (optional)
add_test(NAME timeslice-example-simple-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 events.root)

add_test(NAME timeslice-example-complex-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 timeslices.root)
