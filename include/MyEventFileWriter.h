#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JHasInputs.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTWriter.h>
#include <limits>
#include <map>

struct MyEventFileWriter : public JEventProcessor {

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::string m_output_filename = "merged_output.root";
    size_t m_written_count = 0;
    size_t m_max_events = std::numeric_limits<size_t>::max();
    bool m_write_event_frame = false;
    
    // Accumulators for merging collections across multiple timeslice events
    std::map<std::string, edm4hep::MCParticleCollection> m_merged_mcparticles;
    std::map<std::string, edm4hep::SimTrackerHitCollection> m_merged_tracker_hits;
    std::map<std::string, edm4hep::SimCalorimeterHitCollection> m_merged_calo_hits;
    std::map<std::string, edm4hep::CaloHitContributionCollection> m_merged_calo_contributions;
    std::map<std::string, edm4hep::EventHeaderCollection> m_merged_headers;
    
    size_t m_timeslices_processed = 0;
    size_t m_timeslices_per_merge = 1; // How many timeslices to accumulate before writing

    MyEventFileWriter() {
        SetTypeName(NAME_OF_THIS);
    }

    void Init() override {
        auto app = GetApplication();
        app->GetParameter("writer:nevents", m_max_events);
        app->GetParameter("writer:write_event_frame", m_write_event_frame);
        app->GetParameter("writer:output_filename", m_output_filename);
        app->GetParameter("writer:timeslices_per_merge", m_timeslices_per_merge);

        m_writer = std::make_unique<podio::ROOTWriter>(m_output_filename);
        
        LOG_INFO(GetLogger()) << "MyEventFileWriter: Initialized with output file " << m_output_filename 
                              << ", merging " << m_timeslices_per_merge << " timeslices at a time" << LOG_END;
    }

    void ProcessSequential(const JEvent& event) override {
        
        if (m_written_count >= m_max_events) {
            return;
        }

        if (event.GetLevel() == JEventLevel::Timeslice) {
            std::cout << "Processing timeslice event " << event.GetEventNumber() << " for merging" << std::endl;
            
            // Get the frame from the event
            const auto& ts_frames = event.Get<podio::Frame>();
            if (!ts_frames.empty()) {
                auto& frame = *(ts_frames.at(0));
                auto collection_names = frame.getAvailableCollections();
                std::cout << "Timeslice frame collections: ";
                for (const auto& n : collection_names) std::cout << n << " ";
                std::cout << std::endl;
                
                // Merge collections from this timeslice into accumulators
                for (const auto& coll_name : collection_names) {
                    const podio::CollectionBase* coll = frame.get(coll_name);
                    const auto& coll_type = coll->getValueTypeName();
                    
                    // Determine original collection name by removing "ts_" prefix
                    std::string original_name = coll_name;
                    if (original_name.substr(0, 3) == "ts_") {
                        original_name = original_name.substr(3);
                    }
                    
                    if (coll_type == "edm4hep::MCParticle") {
                        auto typed_coll = frame.get<edm4hep::MCParticleCollection>(coll_name);
                        if (m_merged_mcparticles.find(original_name) == m_merged_mcparticles.end()) {
                            m_merged_mcparticles[original_name] = edm4hep::MCParticleCollection();
                        }
                        for (const auto& particle : *typed_coll) {
                            m_merged_mcparticles[original_name].push_back(particle.clone());
                        }
                    }
                    else if (coll_type == "edm4hep::SimTrackerHit") {
                        auto typed_coll = frame.get<edm4hep::SimTrackerHitCollection>(coll_name);
                        if (m_merged_tracker_hits.find(original_name) == m_merged_tracker_hits.end()) {
                            m_merged_tracker_hits[original_name] = edm4hep::SimTrackerHitCollection();
                        }
                        for (const auto& hit : *typed_coll) {
                            m_merged_tracker_hits[original_name].push_back(hit.clone());
                        }
                    }
                    else if (coll_type == "edm4hep::SimCalorimeterHit") {
                        auto typed_coll = frame.get<edm4hep::SimCalorimeterHitCollection>(coll_name);
                        if (m_merged_calo_hits.find(original_name) == m_merged_calo_hits.end()) {
                            m_merged_calo_hits[original_name] = edm4hep::SimCalorimeterHitCollection();
                        }
                        for (const auto& hit : *typed_coll) {
                            m_merged_calo_hits[original_name].push_back(hit.clone());
                        }
                    }
                    else if (coll_type == "edm4hep::CaloHitContribution") {
                        auto typed_coll = frame.get<edm4hep::CaloHitContributionCollection>(coll_name);
                        if (m_merged_calo_contributions.find(original_name) == m_merged_calo_contributions.end()) {
                            m_merged_calo_contributions[original_name] = edm4hep::CaloHitContributionCollection();
                        }
                        for (const auto& contrib : *typed_coll) {
                            m_merged_calo_contributions[original_name].push_back(contrib.clone());
                        }
                    }
                    else if (coll_type == "edm4hep::EventHeader") {
                        auto typed_coll = frame.get<edm4hep::EventHeaderCollection>(coll_name);
                        if (m_merged_headers.find(original_name) == m_merged_headers.end()) {
                            m_merged_headers[original_name] = edm4hep::EventHeaderCollection();
                        }
                        for (const auto& header : *typed_coll) {
                            m_merged_headers[original_name].push_back(header.clone());
                        }
                    }
                }
                
                m_timeslices_processed++;
                
                // Write merged frame when we've accumulated enough timeslices
                if (m_timeslices_processed >= m_timeslices_per_merge) {
                    WriteMergedFrame();
                    ClearAccumulators();
                    m_timeslices_processed = 0;
                }
            } else {
                LOG_WARN(GetLogger()) << "MyEventFileWriter: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
            }
        }
    }

    void WriteMergedFrame() {
        // Create a new frame with merged collections
        podio::Frame merged_frame;
        
        // Add all merged collections to the frame with original names
        for (auto& [coll_name, coll] : m_merged_mcparticles) {
            merged_frame.put(std::move(coll), coll_name);
        }
        for (auto& [coll_name, coll] : m_merged_tracker_hits) {
            merged_frame.put(std::move(coll), coll_name);
        }
        for (auto& [coll_name, coll] : m_merged_calo_hits) {
            merged_frame.put(std::move(coll), coll_name);
        }
        for (auto& [coll_name, coll] : m_merged_calo_contributions) {
            merged_frame.put(std::move(coll), coll_name);
        }
        for (auto& [coll_name, coll] : m_merged_headers) {
            merged_frame.put(std::move(coll), coll_name);
        }
        
        m_writer->writeFrame(merged_frame, "merged_timeslices");
        m_written_count++;
        
        std::cout << "Wrote merged frame " << m_written_count << " containing collections from " 
                  << m_timeslices_processed << " timeslices" << std::endl;
    }
    
    void ClearAccumulators() {
        m_merged_mcparticles.clear();
        m_merged_tracker_hits.clear();
        m_merged_calo_hits.clear();
        m_merged_calo_contributions.clear();
        m_merged_headers.clear();
    }

    void Finish() override {
        // Write any remaining accumulated data
        if (m_timeslices_processed > 0) {
            WriteMergedFrame();
        }
        
        if (m_writer) {
            m_writer->finish();
        }
        LOG_INFO(GetLogger()) << "MyEventFileWriter: Wrote " << m_written_count << " merged frames to " << m_output_filename << LOG_END;
    }
};