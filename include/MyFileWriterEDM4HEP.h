#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JPodioInput.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTWriter.h>
#include <limits>

struct MyFileWriterEDM4HEP : public JEventProcessor {

    PodioInput<edm4hep::MCParticle> m_ts_MCParticles_in {this, {.name = "ts_hits", .is_optional = true}};
    PodioInput<edm4hep::EventHeader> m_ts_info_in {this, {.name = "ts_info", .is_optional = true}};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::string m_output_filename = "merged_output.root";
    size_t m_written_count = 0;
    size_t m_max_events = std::numeric_limits<size_t>::max();
    bool m_write_event_frame = false;

    MyFileWriterEDM4HEP() {
        SetTypeName(NAME_OF_THIS);
    }

    void Init() override {
        auto app = GetApplication();
        app->GetParameter("writer:nevents", m_max_events);
        app->GetParameter("writer:write_event_frame", m_write_event_frame);
        app->GetParameter("writer:output_filename", m_output_filename);

        m_writer = std::make_unique<podio::ROOTWriter>(m_output_filename);
        
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP: Initialized with output file " << m_output_filename << LOG_END;
    }

    void ProcessSequential(const JEvent& event) override {
        
        if (m_written_count >= m_max_events) {
            return;
        }

        if (event.GetLevel() == JEventLevel::Timeslice) {
            std::cout << "Writing merged timeslice event " << event.GetEventNumber() << std::endl;
            
            // Get the frame from the event
            const auto& ts_frames = event.Get<podio::Frame>();
            if (!ts_frames.empty()) {
                std::cout << "Writing merged timeslice event " << event.GetEventNumber() << std::endl;
                auto names = ts_frames.at(0)->getAvailableCollections();
                std::cout << "Merged timeslice frame collections: ";
                for (const auto& n : names) std::cout << n << " ";
                std::cout << std::endl;
                
                m_writer->writeFrame(*(ts_frames.at(0)), "merged_timeslices");
                m_written_count++;
            } else {
                LOG_WARN(GetLogger()) << "MyFileWriterEDM4HEP: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
            }
            
            // Optionally write the raw parent event frame if requested
            if (m_write_event_frame) {
                //Loop over JEvent levels checking if the frame has a parent
                for(auto level : {JEventLevel::PhysicsEvent, JEventLevel::Subrun}) {
                    if (event.HasParent(level)) {
                        auto& parent = event.GetParent(level);
                        auto* parent_frame = parent.GetSingle<podio::Frame>();
                        if (parent_frame) {
                            m_writer->writeFrame(*parent_frame, "events");
                            LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP: Wrote parent event frame for event " << parent.GetEventNumber() << LOG_END;
                        }
                    }
                }
            }
        }
    }

    void Finish() override {
        if (m_writer) {
            m_writer->finish();
        }
        LOG_INFO(GetLogger()) << "MyFileWriterEDM4HEP: Wrote " << m_written_count << " merged timeslices to " << m_output_filename << LOG_END;
    }
};