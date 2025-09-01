// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

// TimeframeGenerator2 - Generic Timeslice Builder
// Automatically processes any collections available in the event
// Applies consistent time offsets across all collection types

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"

// Conditional EDM4EIC support
#ifdef HAVE_EDM4EIC
#include <edm4eic/ReconstructedParticleCollection.h>
#include <edm4eic/MCRecoParticleAssociationCollection.h>
#endif

#include <random>
#include <map>
#include <any>
#include <typeindex>

struct MyTimesliceBuilder : public JEventUnfolder {

    // Generic collection storage - stores collections by name and type
    struct CollectionData {
        std::vector<std::any> accumulated_events;  // Each element is a vector of objects from one event
        std::type_index type_index;
        std::string collection_name;
        
        CollectionData(std::type_index ti, const std::string& name) 
            : type_index(ti), collection_name(name) {}
    };
    
    std::map<std::string, CollectionData> m_collection_data;
    
    // MCParticles are special - we need them for time offset calculation
    PodioInput<edm4hep::MCParticle> m_event_MCParticles_in {this, {.name = "MCParticles", .is_optional = true}};
    
    size_t parent_idx = 0;
    int events_needed = 0;
    MyTimesliceBuilderConfig m_config;

    // Random number generator members
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniform;
    std::poisson_distribution<> poisson;
    std::normal_distribution<> gaussian;
    
#ifdef HAVE_EDM4EIC
    // Map to track original MCParticles to cloned MCParticles for association updates
    std::map<edm4hep::MCParticle, edm4hep::MCParticle> mcparticle_mapping;
#endif

    MyTimesliceBuilder(MyTimesliceBuilderConfig config)  
          : m_config(config), 
            gen(rd()),
            uniform(0.0f, config.time_slice_duration),
            poisson(config.time_slice_duration * config.mean_hit_frequency),
            gaussian(0.0f, config.beam_spread)
    {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
        if(m_config.static_number_of_hits) {
            events_needed = m_config.static_hits_per_timeslice;
        } else {
            events_needed = poisson(gen);
        }
    }

    template<typename T>
    void RegisterCollectionType(const std::string& collection_name) {
        if (m_collection_data.find(collection_name) == m_collection_data.end()) {
            m_collection_data.emplace(collection_name, CollectionData(std::type_index(typeid(T)), collection_name));
        }
    }
    
    template<typename T>
    void AccumulateCollection(const std::string& collection_name, const JEvent& parent) {
        auto* collection = parent.GetCollection<T>(collection_name, false); // Don't throw if missing
        if (collection) {
            RegisterCollectionType<T>(collection_name);
            
            std::vector<T> event_objects;
            for (const auto& obj : *collection) {
                event_objects.push_back(obj);
            }
            
            m_collection_data[collection_name].accumulated_events.push_back(std::any(event_objects));
        }
    }
    
    template<typename T>
    void ProcessAccumulatedCollection(const std::string& collection_name, JEvent& child, float time_offset, size_t event_idx) {
        auto& coll_data = m_collection_data[collection_name];
        if (event_idx < coll_data.accumulated_events.size()) {
            auto& event_objects = std::any_cast<std::vector<T>&>(coll_data.accumulated_events[event_idx]);
            
            if constexpr (std::is_same_v<T, edm4hep::MCParticle>) {
                auto timeslice_collection = edm4hep::MCParticleCollection();
                for (const auto& obj : event_objects) {
                    auto new_time = obj.getTime() + time_offset;
                    auto new_obj = obj.clone();
                    new_obj.setTime(new_time);
                    new_obj.setGeneratorStatus(obj.getGeneratorStatus() + m_config.generator_status_offset);
                    timeslice_collection.push_back(new_obj);
                    
#ifdef HAVE_EDM4EIC
                    // Track mapping for association updates
                    mcparticle_mapping[obj] = new_obj;
#endif
                }
                child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_collection), "ts_" + collection_name);
            }
            else if constexpr (std::is_same_v<T, edm4hep::SimTrackerHit>) {
                auto timeslice_collection = edm4hep::SimTrackerHitCollection();
                for (const auto& obj : event_objects) {
                    auto new_time = obj.getTime() + time_offset;
                    auto new_obj = obj.clone();
                    new_obj.setTime(new_time);
                    timeslice_collection.push_back(new_obj);
                }
                child.InsertCollection<edm4hep::SimTrackerHit>(std::move(timeslice_collection), "ts_" + collection_name);
            }
#ifdef HAVE_EDM4EIC
            else if constexpr (std::is_same_v<T, edm4eic::ReconstructedParticle>) {
                auto timeslice_collection = edm4eic::ReconstructedParticleCollection();
                for (const auto& obj : event_objects) {
                    auto new_time = obj.getTime() + time_offset;
                    auto new_obj = obj.clone();
                    new_obj.setTime(new_time);
                    timeslice_collection.push_back(new_obj);
                }
                child.InsertCollection<edm4eic::ReconstructedParticle>(std::move(timeslice_collection), "ts_" + collection_name);
            }
            else if constexpr (std::is_same_v<T, edm4eic::MCRecoParticleAssociation>) {
                auto timeslice_collection = edm4eic::MCRecoParticleAssociationCollection();
                for (const auto& obj : event_objects) {
                    auto new_obj = obj.clone();
                    
                    // Update association to reference cloned MCParticle if available
                    auto original_mc = obj.getSim();
                    auto it = mcparticle_mapping.find(original_mc);
                    if (it != mcparticle_mapping.end()) {
                        new_obj.setSim(it->second);
                    }
                    
                    timeslice_collection.push_back(new_obj);
                }
                child.InsertCollection<edm4eic::MCRecoParticleAssociation>(std::move(timeslice_collection), "ts_" + collection_name);
            }
#endif
        }
    }

    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {

        // Always try to get MCParticles for time offset calculation
        auto* particles_in = m_event_MCParticles_in();
        if (!particles_in) {
            std::cerr << "ERROR: MCParticles collection not found! Returning empty collections." << std::endl;
            
            // Create empty output collections
            edm4hep::MCParticleCollection timeslice_particles_out;
            edm4hep::EventHeaderCollection timeslice_info_out;
            
            child.InsertCollection<edm4hep::MCParticle>(std::move(timeslice_particles_out), "ts_MCParticles");
            child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out), "ts_info");
            
            return Result::NextChildNextParent;
        }

        // Accumulate MCParticles
        AccumulateCollection<edm4hep::MCParticle>("MCParticles", parent);
        
        // Try to accumulate known collection types
        AccumulateCollection<edm4hep::SimTrackerHit>("SimTrackerHits", parent);
        
#ifdef HAVE_EDM4EIC
        AccumulateCollection<edm4eic::ReconstructedParticle>("ReconstructedParticles", parent);
        AccumulateCollection<edm4eic::MCRecoParticleAssociation>("MCRecoParticleAssociations", parent);
#endif

        std::cout << parent_idx << std::endl;
        parent_idx++;

        if (m_collection_data["MCParticles"].accumulated_events.size() < events_needed) {
            std::cout << "Not enough particles yet, keep accumulating (need " << events_needed << ", have " << parent_idx << ")" << std::endl;
            return Result::KeepChildNextParent;
        } else if (!m_config.static_number_of_hits) {
            events_needed = poisson(gen);
        }

        parent_idx = 0;

#ifdef HAVE_EDM4EIC
        mcparticle_mapping.clear();
#endif

        // Build the timeslice
        auto timeslice_nr = child_idx;
        child.SetEventNumber(timeslice_nr);

        size_t num_events = m_collection_data["MCParticles"].accumulated_events.size();
        
        for (size_t event_idx = 0; event_idx < num_events; ++event_idx) {
            // Calculate time offset (based on MCParticles logic)
            float time_offset = uniform(gen);
            
            if (m_config.use_bunch_crossing) {
                time_offset = std::floor(time_offset / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
            }
            
            if (m_config.attach_to_beam) {
                time_offset += gaussian(gen);
                
                // Get MCParticles for this event to find vertex
                auto& mcparticles_event = std::any_cast<std::vector<edm4hep::MCParticle>&>(
                    m_collection_data["MCParticles"].accumulated_events[event_idx]);
                    
                auto first_particle = std::find_if(mcparticles_event.begin(), mcparticles_event.end(), 
                    [](const auto& p) { return p.getGeneratorStatus() == 1; });
                    
                if (first_particle != mcparticles_event.end()) {
                    float distance = std::sqrt(std::pow(first_particle->getVertex().x, 2) +
                                                std::pow(first_particle->getVertex().y, 2) +
                                                std::pow(first_particle->getVertex().z, 2));
                    time_offset += distance / m_config.beam_speed;
                }
            }

            // Process all accumulated collections with the same time offset
            for (auto& [coll_name, coll_data] : m_collection_data) {
                if (coll_data.type_index == std::type_index(typeid(edm4hep::MCParticle))) {
                    ProcessAccumulatedCollection<edm4hep::MCParticle>(coll_name, child, time_offset, event_idx);
                }
                else if (coll_data.type_index == std::type_index(typeid(edm4hep::SimTrackerHit))) {
                    ProcessAccumulatedCollection<edm4hep::SimTrackerHit>(coll_name, child, time_offset, event_idx);
                }
#ifdef HAVE_EDM4EIC
                else if (coll_data.type_index == std::type_index(typeid(edm4eic::ReconstructedParticle))) {
                    ProcessAccumulatedCollection<edm4eic::ReconstructedParticle>(coll_name, child, time_offset, event_idx);
                }
                else if (coll_data.type_index == std::type_index(typeid(edm4eic::MCRecoParticleAssociation))) {
                    ProcessAccumulatedCollection<edm4eic::MCRecoParticleAssociation>(coll_name, child, time_offset, event_idx);
                }
#endif
            }
        }

        // Create timeslice info
        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(timeslice_nr);
        header.setRunNumber(0);
        header.setTimeStamp(timeslice_nr);
        
        edm4hep::EventHeaderCollection timeslice_info_out;
        timeslice_info_out.push_back(header);
        child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out), "ts_info");

        // Clear accumulators for next timeslice
        for (auto& [coll_name, coll_data] : m_collection_data) {
            coll_data.accumulated_events.clear();
        }

        return Result::NextChildNextParent;
    }
};