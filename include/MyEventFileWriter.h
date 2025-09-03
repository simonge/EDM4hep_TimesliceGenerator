#pragma once

#include <JANA/JEventProcessor.h>
#include <JANA/Components/JHasInputs.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>

struct MyEventFileWriter : public JEventProcessor {

    // Remove dependency on merged MCParticles - we'll merge them ourselves
    // PodioInput<edm4hep::MCParticle> m_MCParticles_in  {this, {.name="MCParticles", .level = JEventLevel::PhysicsEvent}};

    Input<podio::Frame> m_frame_in {this, {.name = "", .level = JEventLevel::PhysicsEvent}};

    std::unique_ptr<podio::ROOTWriter> m_writer = nullptr;
    std::mutex m_mutex;
    std::string m_output_filename = "merged_output.root";
    size_t m_written_count = 0;
    size_t m_max_events = std::numeric_limits<size_t>::max();
        
    size_t m_timeslices_processed = 0;

    MyEventFileWriter() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
        SetLevel(JEventLevel::PhysicsEvent);
    }

    void Init() override {
        auto app = GetApplication();
        app->GetParameter("writer:nevents", m_max_events);
        app->GetParameter("writer:output_filename", m_output_filename);

        m_writer = std::make_unique<podio::ROOTWriter>(m_output_filename);
        
    }

    void ProcessSequential(const JEvent& event) override {
        
        if (m_written_count >= m_max_events) {
            // Stop further writing and signal JANA to stop
            auto* app = GetApplication();
            if (app) app->Stop();
            return;
        }

        std::cout << "Processing event " << event.GetEventNumber() << std::endl;
            
        // Get the frame from the event (now contains both original ts_* and merged collections)
        const auto& ts_frames = m_frame_in();
        if (!ts_frames.empty()) {
            const auto& frame = *(ts_frames.at(0));
            
            // Write the frame directly - it now contains both original and merged collections
            m_writer->writeFrame(frame, "events");     
            
            m_written_count++;
            
        } else {
            LOG_WARN(GetLogger()) << "MyEventFileWriter: No timeslice frame available for timeslice event " << event.GetEventNumber() << LOG_END;
        }
        
    }

private:

    void Finish() override {
        m_writer->finish();
    }
};