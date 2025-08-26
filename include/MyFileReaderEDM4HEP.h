// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventSource.h>
// #include <JANA/Components/JHasInputs.h>
#include <JANA/Components/JPodioOutput.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"


struct MyFileReaderEDM4HEP : public JEventSource {

    // PodioOutput<edm4hep::SimTrackerHit> m_hits_out {this, "hits"};

    MyFileReaderEDM4HEP() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    std::string m_tag{""};

    void SetTag(std::string tag) { m_tag = std::move(tag); }
    const std::string& GetTag() const { return m_tag; }

    void Open() override { }

    void Close() override { }

    Result Emit(JEvent& event) override {
        auto event_nr = event.GetEventNumber();
        edm4hep::SimTrackerHitCollection hits_out;

        float time = static_cast<float>(event_nr);
        //If level is Subevent, make time negative to distinguish
        if(event.GetLevel() == JEventLevel::Subevent) time = -time;

        // Emit 3 hits per event/timeslice
        edm4hep::MutableSimTrackerHit hit1;
        hit1.setCellID(event_nr);
        hit1.setEDep(22.0);
        hit1.setTime(time);
        hit1.setPosition({22.0, 22.0, 22.0});
        hits_out.push_back(hit1);

        edm4hep::MutableSimTrackerHit hit2;
        hit2.setCellID(event_nr);
        hit2.setEDep(49.0);
        hit2.setTime(time);
        hit2.setPosition({49.0, 49.0, 49.0});
        hits_out.push_back(hit2);

        edm4hep::MutableSimTrackerHit hit3;
        hit3.setCellID(event_nr);
        hit3.setEDep(7.6);
        hit3.setTime(time);
        hit3.setPosition({7.6, 7.6, 7.6});
        hits_out.push_back(hit3);

        LOG_DEBUG(GetLogger()) << "MySource: Emitted " << GetLevel() << " " << event.GetEventNumber() << "\n"
            << TabulateHitsEDM4HEP(&hits_out)
            << LOG_END;

        std::cout << "Emitting event " << event_nr << " with " << hits_out.size() << " hits." << std::endl;
        std::cout << "At Level " << static_cast<int>(event.GetLevel()) << " Tag " << m_tag << std::endl;

        event.InsertCollection<edm4hep::SimTrackerHit>(std::move(hits_out), m_tag+"hits");

        edm4hep::EventHeaderCollection info;
        info.push_back(edm4hep::MutableEventHeader(event_nr, 0, 0, 0));
        event.InsertCollection<edm4hep::EventHeader>(std::move(info), m_tag+"evt_info");
        return Result::Success;
    }
};
