// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JHasInputs.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/MCParticleCollection.h>
#include <podio/podioVersion.h>
#include <podio/ROOTWriter.h>

struct MyTimesliceFileWriter : public JEventProcessor {

    PodioInput<edm4hep::MCParticle> m_ts_MCParticles_in  {this, {.name="MCParticles", .level = JEventLevel::Timeslice}};

    Input<podio::Frame> m_ts_frame_in {this, {.name = "", 
                                              .level = JEventLevel::Timeslice}};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::mutex m_mutex;
    std::string m_output_name;
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
        // Get max events from config
        auto* app = GetApplication();
        if (app) {
            m_output_name = app->GetParameterValue<std::string>("output_file");
            m_max_events  = app->GetParameterValue<size_t>("writer:nevents");
            LOG_INFO(GetLogger()) << "MyTimesliceFileWriter: Output event limit set to " << m_max_events << LOG_END;
            LOG_INFO(GetLogger()) << "MyTimesliceFileWriter: Write parent event frame: " << m_write_event_frame << LOG_END;
        }
        
        m_writer = std::make_unique<podio::ROOTWriter>(m_output_name);
    }

    void ProcessSequential(const JEvent& event) override {
        if (m_written_count >= m_max_events) {
            // Stop further writing and signal JANA to stop
            auto* app = GetApplication();
            if (app) app->Stop();
            return;
        }
        // std::cout << "Processing seq event " << event.GetEventNumber() << std::endl;
        LOG_INFO(GetLogger()) << "MyTimesliceFileWriter::ProcessSequential called for event " << event.GetEventNumber() << LOG_END;
        std::lock_guard<std::mutex> guard(m_mutex);

        // If this is a timeslice event, write the timeslice frame
        if (event.GetLevel() == JEventLevel::Timeslice) {
            const auto& ts_frames = m_ts_frame_in();
            if (!ts_frames.empty()) {
                // auto names = ts_frames.at(0)->getAvailableCollections();
                m_writer->writeFrame(*(ts_frames.at(0)), "events");
                m_written_count++;
            } else {
                LOG_WARN(GetLogger()) << "MyTimesliceFileWriter: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
            }
        }
    }

    void Finish() override {
        m_writer->finish();
    }
};
