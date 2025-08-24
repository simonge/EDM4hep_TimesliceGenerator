// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventProcessor.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"

#include <podio/podioVersion.h>
#include <podio/Frame.h>
#include <podio/ROOTWriter.h>



struct MyFileWriterEDM4HEP : public JEventProcessor {

    // Trigger the creation of clusters
    PodioInput<edm4hep::CalorimeterHit> m_evt_hits_in {this, {.name="ev_hits"}};

    // Retrieve the PODIO frame so we can write it directly
    Input<podio::Frame> m_evt_frame_in {this, {.name = "", 
                                               .level = JEventLevel::PhysicsEvent}};

    Input<podio::Frame> m_ts_frame_in {this, {.name = "", 
                                              .level = JEventLevel::Timeslice,
                                              .is_optional = true }};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::mutex m_mutex;
    
    MyFileWriterEDM4HEP() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP constructed" << LOG_END;
    }

    void Init() override {
        m_writer = std::make_unique<podio::ROOTWriter>("output.root");
    }

    // Ensure this processor is actually called by the engine
    void Process(const JEvent& event) override {
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP::Process called for event " << event.GetEventNumber() << LOG_END;
        ProcessSequential(event);
    }

    void ProcessSequential(const JEvent& event) {
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP::ProcessSequential called for event " << event.GetEventNumber() << LOG_END;
        std::lock_guard<std::mutex> guard(m_mutex);
        // Diagnostics: check event frame contents
        const auto& evt_frames = m_evt_frame_in();
        if (evt_frames.empty()) {
            LOG_WARN(GetLogger()) << "MyFileWriterEDM4HEP: No event frame available for event "
                                  << event.GetEventNumber() << LOG_END;
        } else {
            const auto& f = *(evt_frames.at(0));
            auto names = f.getAvailableCollections();
            std::ostringstream oss; for (auto& n : names) oss << n << " ";
            LOG_DEBUG(GetLogger()) << "MyFileWriterEDM4HEP: Event frame collections: " << oss.str() << LOG_END;
        }
        if (event.HasParent(JEventLevel::Timeslice)) {

            auto& ts = event.GetParent(JEventLevel::Timeslice);
            auto ts_nr = ts.GetEventNumber();

            if (event.GetEventIndex() == 0) {
                const auto& ts_frames = m_ts_frame_in();
                if (ts_frames.empty()) {
                    LOG_WARN(GetLogger()) << "MyFileWriterEDM4HEP: No timeslice frame available for timeslice "
                                          << ts_nr << LOG_END;
                } else {
                    auto names = ts_frames.at(0)->getAvailableCollections();
                    std::ostringstream oss; for (auto& n : names) oss << n << " ";
                    LOG_DEBUG(GetLogger()) << "MyFileWriterEDM4HEP: Timeslice frame collections: " << oss.str() << LOG_END;
                    m_writer->writeFrame(*(ts_frames.at(0)), "timeslices");
                }
            }

            LOG_DEBUG(GetLogger()) 
                << "Event " << event.GetEventNumber() << " from Timeslice " << ts_nr
                << "\nHits\n"
                << TabulateHitsEDM4HEP(m_evt_hits_in())
                << LOG_END;
        }
        else {

            LOG_DEBUG(GetLogger()) 
                << "Event " << event.GetEventNumber()
                << "\nClusters\n"
                << TabulateHitsEDM4HEP(m_evt_hits_in())
                << LOG_END;
        }

        if (!m_evt_frame_in().empty()) {
            m_writer->writeFrame(*(m_evt_frame_in().at(0)), "events");
        }

    }

    void Finish() override {
        m_writer->finish();
    }
};
