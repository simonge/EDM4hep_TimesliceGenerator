cmake_minimum_required(VERSION 3.16)

# This CMakeLists.txt builds the TimeframeBuilder JANA2 plugin
# It is designed to be optionally built when JANA2 is available

# Find JANA2 (optional)
find_package(JANA QUIET)

if(JANA_FOUND)
    message(STATUS "JANA2 found - building TimeframeBuilder JANA2 plugin")
    
    # Set the plugin name
    set(PLUGIN_NAME timeframe_builder_plugin)
    
    # Create the plugin library
    add_library(${PLUGIN_NAME} SHARED
        timeframe_builder_plugin.cc
        JEventSourceTimeframeBuilderEDM4hep.cc
        JEventSourceTimeframeBuilderHepMC3.cc
    )
    
    # Include directories
    target_include_directories(${PLUGIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${JANA_INCLUDE_DIR}
        ${ROOT_INCLUDE_DIRS}
    )
    
    # Link against required libraries
    target_link_libraries(${PLUGIN_NAME}
        ${JANA_LIBRARIES}
        ${ROOT_LIBRARIES}
        podio::podioRootIO
        EDM4HEP::edm4hep
        EDM4HEP::edm4hepDict
        yaml-cpp
    )
    
    # Add HepMC3 support if available
    if(HepMC3_FOUND)
        target_compile_definitions(${PLUGIN_NAME} PRIVATE HAVE_HEPMC3)
        target_link_libraries(${PLUGIN_NAME} HepMC3::HepMC3)
        if(TARGET HepMC3::rootIO)
            target_link_libraries(${PLUGIN_NAME} HepMC3::rootIO)
        endif()
    endif()
    
    # We need to link against the TimeframeBuilder library objects
    # Since we're building sources directly, we need to add all the source files
    target_sources(${PLUGIN_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/DataSource.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/EDM4hepDataSource.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/DataHandler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/EDM4hepDataHandler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/TimeframeBuilder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/CommandLineParser.cc
    )
    
    # Add HepMC3 sources if available
    if(HepMC3_FOUND)
        target_sources(${PLUGIN_NAME} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/HepMC3DataSource.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/HepMC3DataHandler.cc
        )
    endif()
    
    # Set C++ standard
    set_target_properties(${PLUGIN_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
    
    # Install the plugin
    install(TARGETS ${PLUGIN_NAME}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
    
    # Installation for JANA2 plugin directory (if JANA_PLUGIN_PATH is set)
    if(DEFINED ENV{JANA_PLUGIN_PATH})
        install(TARGETS ${PLUGIN_NAME}
            LIBRARY DESTINATION $ENV{JANA_PLUGIN_PATH}
        )
    endif()
    
else()
    message(STATUS "JANA2 not found - skipping TimeframeBuilder JANA2 plugin")
endif()
