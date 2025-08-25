// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/ClusterCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"

struct MyTimesliceSplitterEDM4HEP : public JEventUnfolder {

    PodioInput<edm4hep::CalorimeterHit> m_timeslice_hits_in {this, {.name = "hits", .level = JEventLevel::Timeslice}};
    PodioOutput<edm4hep::CalorimeterHit> m_event_hits_out {this, "ev_hits"};
    PodioOutput<edm4hep::EventHeader> m_event_info_out {this, "evt_info"};

    MyTimesliceSplitterEDM4HEP() {
        SetTypeName(NAME_OF_THIS);
        SetParentLevel(JEventLevel::Timeslice);
        SetChildLevel(JEventLevel::PhysicsEvent);
    }


    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {
        auto timeslice_nr = parent.GetEventNumber();
        size_t event_nr = 100*timeslice_nr + child_idx;
        child.SetEventNumber(event_nr);

        // Each child event gets one hit from the timeslice
        auto event_hits_out = std::make_unique<edm4hep::CalorimeterHitCollection>();
        // event_hits_out->setSubsetCollection(true);
        auto& hits_in = *m_timeslice_hits_in();
        if (child_idx < hits_in.size()) {
            event_hits_out->push_back(hits_in.at(child_idx).clone());
        }

        auto event_info_out = std::make_unique<edm4hep::EventHeaderCollection>();
        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(event_nr);
        header.setRunNumber(0);
        header.setTimeStamp(timeslice_nr);
        event_info_out->push_back(header);

        LOG_DEBUG(GetLogger()) << "MyTimesliceSplitter: Timeslice " << parent.GetEventNumber()
            <<  ", Event " << child.GetEventNumber()
            << "\nTimeslice hits in:\n"
            << TabulateHitsEDM4HEP(&hits_in)
            << "\nEvent hits out:\n"
            << TabulateHitsEDM4HEP(event_hits_out.get())
            << LOG_END;

        m_event_hits_out() = std::move(event_hits_out);
        m_event_info_out() = std::move(event_info_out);

        // Only 3 events per timeslice
        return (child_idx == 2) ? Result::NextChildNextParent : Result::NextChildKeepParent;
    }
};



