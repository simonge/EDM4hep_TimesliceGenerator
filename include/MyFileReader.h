// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventSource.h>
#include <PodioDatamodel/TimesliceInfoCollection.h>
#include <PodioDatamodel/EventInfoCollection.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include "CollectionTabulators.h"


struct MyFileReader : public JEventSource {

    PodioOutput<edm4hep::CalorimeterHit> m_hits_out {this, "hits"};

    MyFileReader() {
        SetTypeName(NAME_OF_THIS);
        SetCallbackStyle(CallbackStyle::ExpertMode);
    }

    void Open() override { }

    void Close() override { }

    Result Emit(JEvent& event) override {

        auto event_nr = event.GetEventNumber();

        auto hits_out  = std::make_unique<edm4hep::CalorimeterHitCollection>();

        // CalorimeterHit: cellID, energy, time, position
        auto hit1 = edm4hep::MutableCalorimeterHit();
        hit1.setCellID(event_nr);
        hit1.setEnergy(22.0);
        hit1.setTime(0.0);
        hit1.setPosition({22.0, 22.0, 22.0});
        hits_out->push_back(hit1);

        auto hit2 = edm4hep::MutableCalorimeterHit();
        hit2.setCellID(event_nr);
        hit2.setEnergy(49.0);
        hit2.setTime(1.0);
        hit2.setPosition({49.0, 49.0, 49.0});
        hits_out->push_back(hit2);

        auto hit3 = edm4hep::MutableCalorimeterHit();
        hit3.setCellID(event_nr);
        hit3.setEnergy(7.6);
        hit3.setTime(2.0);
        hit3.setPosition({7.6, 7.6, 7.6});
        hits_out->push_back(hit3);

        LOG_DEBUG(GetLogger()) << "MySource: Emitted " << GetLevel() << " " << event.GetEventNumber() << "\n"
            << TabulateHits(hits_out.get())
            << LOG_END;

        m_hits_out() = std::move(hits_out);

        // Furnish this event with info object
        if (GetLevel() == JEventLevel::Timeslice) {
            TimesliceInfoCollection info;
            info.push_back(MutableTimesliceInfo(event_nr, 0)); // event nr, run nr
            event.InsertCollection<TimesliceInfo>(std::move(info), "ts_info");
        }
        else {
            EventInfoCollection info;
            info.push_back(MutableEventInfo(event_nr, 0, 0)); // event nr, timeslice nr, run nr
            event.InsertCollection<EventInfo>(std::move(info), "evt_info");
        }
        return Result::Success;
    }
};
