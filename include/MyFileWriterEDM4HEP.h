// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JHasInputs.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"

#include <podio/podioVersion.h>
#include <podio/ROOTWriter.h>

struct MyFileWriterEDM4HEP : public JEventProcessor {

    // Trigger the creation of clusters
    // PodioInput<edm4hep::CalorimeterHit> m_evt_hits_in {this, {.name="hits", .level = JEventLevel::PhysicsEvent}};
    PodioInput<edm4hep::CalorimeterHit> m_ts_hits_in  {this, {.name="ts_hits", .level = JEventLevel::Timeslice}};

    // Retrieve the PODIO frame so we can write it directly
    Input<podio::Frame> m_evt_frame_in {this, {.name = "", 
                                               .level = JEventLevel::PhysicsEvent,
                                               .is_optional = true }};

    Input<podio::Frame> m_ts_frame_in {this, {.name = "", 
                                              .level = JEventLevel::Timeslice}};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::mutex m_mutex;
    
    MyFileWriterEDM4HEP() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        SetLevel(JEventLevel::Timeslice);
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP constructed" << LOG_END;
    }

    void Init() override {
        m_writer = std::make_unique<podio::ROOTWriter>("output.root");
    }

    void ProcessSequential(const JEvent& event) override {
        std::cout << "Processing seq event " << event.GetEventNumber() << std::endl;
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP::ProcessSequential called for event " << event.GetEventNumber() << LOG_END;
        std::lock_guard<std::mutex> guard(m_mutex);

        std::cout << "Event " << event.GetEventNumber() << " at level " << event.GetLevel() << std::endl;
        // Write physics event frame if present
        // if (event.GetLevel() == JEventLevel::PhysicsEvent) {
        //     const auto& evt_frames = m_evt_frame_in();
        //     if (!evt_frames.empty()) {
        //         const auto& f = *(evt_frames.at(0));
        //         auto names = f.getAvailableCollections();
        //         std::cout << "PhysicsEvent frame collections: ";
        //         for (const auto& n : names) std::cout << n << " ";
        //         std::cout << std::endl;
        //         m_writer->writeFrame(f, "events");
        //     } else {
        //         std::cout << "No PhysicsEvent frame available for event " << event.GetEventNumber() << std::endl;
        //     }
            // Optionally, log hits
            // LOG_DEBUG(GetLogger())
            //     << "Physics Event " << event.GetEventNumber()
            //     << "\nHits\n"
            //     << TabulateHitsEDM4HEP(m_evt_hits_in())
            //     << LOG_END;
        // }
    // Write event frame if present
        // const auto& evt_frames = m_evt_frame_in();
        // if (!evt_frames.empty()) {
        //     std::cout << "Writing event " << event.GetEventNumber() << std::endl;
        //     const auto& f = *(evt_frames.at(0));
        //     auto names = f.getAvailableCollections();
        //     std::ostringstream oss; for (auto& n : names) oss << n << " ";
        //     LOG_DEBUG(GetLogger()) << "MyFileWriterEDM4HEP: Event frame collections: " << oss.str() << LOG_END;
        //     m_writer->writeFrame(f, "events");
        // } else {
        //     LOG_WARN(GetLogger()) << "MyFileWriterEDM4HEP: No event frame available for event " << event.GetEventNumber() << LOG_END;
        // }

        std::cout << "Number of parents " << event.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;
        // If this is a timeslice event, write the timeslice frame
        if (event.GetLevel() == JEventLevel::Timeslice) {

            // // NEW: Loop through parent events and write their frames if present
            // std::cout << parents.size() << " parent events found." << std::endl;
            // auto parents = const_cast<JEvent&>(event).ReleaseAllParents();
            // for (const auto* parent : parents) {
            //     if (parent == nullptr) continue;
            //     // Try to get the podio frame from the parent event
            //     auto parent_frames = parent->Get<podio::Frame>();
            //     if (!parent_frames.empty()) {
            //         const auto& pf = *(parent_frames.at(0));
            //         auto parent_names = pf.getAvailableCollections();
            //         std::cout << "Parent event " << parent->GetEventNumber() << " frame collections: ";
            //         for (const auto& n : parent_names) std::cout << n << " ";
            //         std::cout << std::endl;
            //         m_writer->writeFrame(pf, "events");
            //     } else {
            //         std::cout << "No podio frame found for parent event " << parent->GetEventNumber() << std::endl;
            //     }
            // }


            std::cout << "Writing timeslice event " << event.GetEventNumber() << " with " << m_ts_hits_in()->size() << " hits." << std::endl;
            const auto& ts_frames = m_ts_frame_in();
            if (!ts_frames.empty()) {
                std::cout << "Writing timeslice event " << event.GetEventNumber() << std::endl;
                auto names = ts_frames.at(0)->getAvailableCollections();
                std::cout << "Timeslice frame collections: ";
                for (const auto& n : names) std::cout << n << " ";
                std::ostringstream oss; for (auto& n : names) oss << n << " ";
                LOG_DEBUG(GetLogger()) << "MyFileWriterEDM4HEP: Timeslice frame collections: " << oss.str() << LOG_END;
                m_writer->writeFrame(*(ts_frames.at(0)), "timeslices");
            } else {
                LOG_WARN(GetLogger()) << "MyFileWriterEDM4HEP: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
            }



            // LOG_DEBUG(GetLogger()) 
            //     << "Timeslice Event " << event.GetEventNumber()
            //     << "\nHits\n"
            //     << TabulateHitsEDM4HEP(m_ts_hits_in())
            //     << LOG_END;
        }
    }

    void Finish() override {
        m_writer->finish();
    }
};
