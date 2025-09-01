cmake_minimum_required(VERSION 3.16)
project(TimesliceGenerator VERSION 1.0.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JANA
find_package(JANA REQUIRED)

# Find PODIO (required for TimesliceGenerator)
find_package(podio REQUIRED)

# Find EDM4HEP
find_package(EDM4HEP REQUIRED)

# Find EDM4EIC (optional)
find_package(EDM4EIC QUIET)
if(EDM4EIC_FOUND)
    message(STATUS "Found EDM4EIC, enabling EDM4EIC support")
    add_definitions(-DHAVE_EDM4EIC)
    set(EDM4EIC_LIBRARIES EDM4EIC::edm4eic)
else()
    message(STATUS "EDM4EIC not found, disabling EDM4EIC support")
    set(EDM4EIC_LIBRARIES "")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create the plugin
add_jana_plugin(TimesliceCreator
    SOURCES
        src/TimesliceCreator.cc
        src/CollectionTabulatorsEDM4HEP.cc
    PUBLIC_HEADER
        include/CollectionTabulatorsEDM4HEP.h
        include/MyEventFileReader.h
        include/MyEventFileReaderGenerator.h
        include/MyTimesliceFileWriter.h
        include/MyTimesliceBuilder.h
)

# Create the plugin
# add_jana_plugin(TimesliceMerger
#     SOURCES
#         src/TimesliceMerger.cc
#         src/CollectionTabulatorsEDM4HEP.cc
#     PUBLIC_HEADER
#         include/MyTimesliceFileReader.h
#         include/MyTimesliceFileReaderGenerator.h
#         include/MyFileWriterEDM4HEP.h
#         include/SimTrackerHitCollector_factory.h
# )

# Link against PODIO and EDM4HEP
target_link_libraries(TimesliceCreator PUBLIC
    podio::podioRootIO
    EDM4HEP::edm4hep
    EDM4HEP::edm4hepDict
    ${EDM4EIC_LIBRARIES}
)

# Link against PODIO and EDM4HEP
# target_link_libraries(TimesliceMerger PUBLIC
#     podio::podioRootIO
#     EDM4HEP::edm4hep
#     EDM4HEP::edm4hepDict
# )

# Set install prefix if not specified
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "Installation directory" FORCE)
endif()

# Add tests (optional)
add_test(NAME timeslice-example-simple-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 events.root)

add_test(NAME timeslice-example-complex-test
    COMMAND ${CMAKE_INSTALL_PREFIX}/bin/jana -Pplugins=EDM4HEPTimesliceExample -Pjana:nevents=10 timeslices.root)
