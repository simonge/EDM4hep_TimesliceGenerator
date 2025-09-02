// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JHasInputs.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/MCParticleCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"

#include <podio/podioVersion.h>
#include <podio/ROOTWriter.h>

struct MyTimesliceFileWriter : public JEventProcessor {

    // Trigger the creation of clusters
    // PodioInput<edm4hep::MCParticle> m_evt_MCParticles_in {this, {.name="MCParticles", .level = JEventLevel::PhysicsEvent}};
    PodioInput<edm4hep::MCParticle> m_ts_MCParticles_in  {this, {.name="ts_MCParticles", .level = JEventLevel::Timeslice}};

    // Retrieve the PODIO frame so we can write it directly
    // Input<podio::Frame> m_evt_frame_in {this, {.name = "", 
    //                                            .level = JEventLevel::PhysicsEvent,
    //                                            .is_optional = true }};

    Input<podio::Frame> m_ts_frame_in {this, {.name = "", 
                                              .level = JEventLevel::Timeslice}};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::mutex m_mutex;
        size_t m_written_count = 0;
        size_t m_max_events = std::numeric_limits<size_t>::max();
        bool m_write_event_frame = false;
    
    MyTimesliceFileWriter() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        SetLevel(JEventLevel::Timeslice);
        LOG_INFO(GetLogger()) << "MyTimesliceFileWriter constructed" << LOG_END;
    }

    void Init() override {
        m_writer = std::make_unique<podio::ROOTWriter>("output.root");
        // Get max events from config
        auto* app = GetApplication();
        if (app) {
            m_max_events = app->GetParameterValue<size_t>("writer:nevents");
            m_write_event_frame = app->GetParameterValue<bool>("writer:write_event_frame");
            LOG_INFO(GetLogger()) << "MyTimesliceFileWriter: Output event limit set to " << m_max_events << LOG_END;
            LOG_INFO(GetLogger()) << "MyTimesliceFileWriter: Write parent event frame: " << m_write_event_frame << LOG_END;
        }
    }

    void ProcessSequential(const JEvent& event) override {
        if (m_written_count >= m_max_events) {
            // Stop further writing and signal JANA to stop
            auto* app = GetApplication();
            if (app) app->Stop();
            return;
        }
        std::cout << "Processing seq event " << event.GetEventNumber() << std::endl;
        LOG_INFO(GetLogger()) << "MyTimesliceFileWriter::ProcessSequential called for event " << event.GetEventNumber() << LOG_END;
        std::lock_guard<std::mutex> guard(m_mutex);

        std::cout << "Event " << event.GetEventNumber() << " at level " << event.GetLevel() << std::endl;

        std::cout << "Number of parents " << event.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;
        // If this is a timeslice event, write the timeslice frame
        if (event.GetLevel() == JEventLevel::Timeslice) {
            std::cout << "Writing timeslice event " << event.GetEventNumber() << " with " << m_ts_MCParticles_in()->size() << " particles." << std::endl;
            const auto& ts_frames = m_ts_frame_in();
            if (!ts_frames.empty()) {
                std::cout << "Writing timeslice event " << event.GetEventNumber() << std::endl;
                auto names = ts_frames.at(0)->getAvailableCollections();
                std::cout << "Timeslice frame collections: ";
                for (const auto& n : names) std::cout << n << " ";
                std::ostringstream oss; for (auto& n : names) oss << n << " ";
                LOG_DEBUG(GetLogger()) << "MyTimesliceFileWriter: Timeslice frame collections: " << oss.str() << LOG_END;
                m_writer->writeFrame(*(ts_frames.at(0)), "timeslices");
                m_written_count++;
            } else {
                LOG_WARN(GetLogger()) << "MyTimesliceFileWriter: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
            }
            // Optionally write the raw parent event frame if requested
            // if (m_write_event_frame) {
            //     //Loop over JEvent levels checking if the frame has a parent
            //     for(auto level : {JEventLevel::PhysicsEvent, JEventLevel::Subrun}) {
            //         if (event.HasParent(level)) {
            //             auto& parent = event.GetParent(level);
            //             auto* parent_frame = parent.GetSingle<podio::Frame>();
            //             if (parent_frame) {
            //                 m_writer->writeFrame(*parent_frame, "events");
            //                 LOG_INFO(GetLogger()) << "MyTimesliceFileWriter: Wrote parent PhysicsEvent frame for event " << parent.GetEventNumber() << LOG_END;
            //             }
            //         }
            //     }
            // }
        }
    }

    void Finish() override {
        m_writer->finish();
    }
};
