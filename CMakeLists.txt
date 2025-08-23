cmake_minimum_required(VERSION 3.16)
project(TimesliceExample VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JANA
find_package(JANA REQUIRED)

# Find PODIO (required for TimesliceExample)
find_package(podio REQUIRED)

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the plugin
add_jana_plugin(TimesliceExample
    SOURCES
        src/TimesliceExample.cc
        src/CollectionTabulators.cc
    PUBLIC_HEADER
        include/CollectionTabulators.h
        include/MyClusterFactory.h
        include/MyFileReader.h
        include/MyFileReaderGenerator.h
        include/MyFileWriter.h
        include/MyProtoclusterFactory.h
        include/MyTimesliceSplitter.h
)

# Link against PODIO and EDM4HEP
target_link_libraries(TimesliceExample PUBLIC 
    PodioDatamodel 
    PodioDatamodelDict 
    podio::podioRootIO
    EDM4HEP::edm4hep
)

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Add tests (optional)
add_test(NAME timeslice-example-simple-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=TimesliceExample -Pjana:nevents=10 events.root)

add_test(NAME timeslice-example-complex-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=TimesliceExample -Pjana:nevents=10 timeslices.root)
