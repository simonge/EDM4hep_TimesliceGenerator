// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/JEventUnfolder.h>
#include <edm4hep/EventHeaderCollection.h>
#include "CollectionTabulatorsEDM4HEP.h"
#include "MyTimesliceBuilderConfig.h"



struct MyTimesliceBuilderEDM4HEP : public JEventUnfolder {

    PodioInput<edm4hep::SimTrackerHit> m_event_hits_in {this, {.name = "hits"}};
    // PodioOutput<edm4hep::SimTrackerHit> m_timeslice_hits_out {this, "ts_hits"};
    // PodioOutput<edm4hep::EventHeader> m_timeslice_info_out {this, "ts_info"};

    std::vector<edm4hep::SimTrackerHit> hit_accumulator;
    size_t parent_idx = 0;

    MyTimesliceBuilderConfig m_config;

    MyTimesliceBuilderEDM4HEP(MyTimesliceBuilderConfig config) : m_config(config) {
        SetTypeName(NAME_OF_THIS);
        SetChildLevel(JEventLevel::Timeslice);
        SetParentLevel(m_config.parent_level);
        m_event_hits_in.SetCollectionName(m_config.tag + "hits");
        // m_timeslice_hits_out.SetCollectionName(m_config.tag + "ts_hits");
        // m_timeslice_info_out.SetCollectionName(m_config.tag + "ts_info");
    }

    Result Unfold(const JEvent& parent, JEvent& child, int child_idx) override {

        // Accumulate hits from each parent event
        auto& hits_in = *m_event_hits_in();

        std::cout << parent_idx << std::endl;


        for (const auto& hit : hits_in) {
            std::cout << "AcumulatedHits " << hit_accumulator.size() << std::endl;
            LOG_DEBUG(GetLogger()) << "NHits: " << hit_accumulator.size()
                << "\nCurrent hits:\n"
                << LOG_END;
            hit_accumulator.push_back(hit);
        }
        
        if (parent_idx < 2) {
            parent_idx++;
            // Not enough hits yet, keep accumulating
            std::cout << "Not enough hits yet, keep accumulating" << std::endl;
            return Result::KeepChildNextParent;
        }

        parent_idx = 0;

        edm4hep::SimTrackerHitCollection timeslice_hits_out;
        edm4hep::EventHeaderCollection   timeslice_info_out;

        // Now we have 3 hits, build the timeslice
        auto timeslice_nr = child_idx;//1000+parent.GetEventNumber() / 3;
        child.SetEventNumber(timeslice_nr);
        // child.SetParent(const_cast<JEvent*>(&parent));
        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        for (const auto& hit : hit_accumulator) {
            timeslice_hits_out.push_back(hit.clone());
        }

        auto header = edm4hep::MutableEventHeader();
        header.setEventNumber(timeslice_nr);
        header.setRunNumber(0);
        header.setTimeStamp(timeslice_nr);
        timeslice_info_out.push_back(header);

        LOG_DEBUG(GetLogger()) << "MyTimesliceBuilder: Built timeslice " << timeslice_nr
            << "\nTimeslice hits out:\n"
            << TabulateHitsEDM4HEP(&timeslice_hits_out)
            << LOG_END;

        child.InsertCollection<edm4hep::SimTrackerHit>(std::move(timeslice_hits_out),m_config.tag + "ts_hits");
        child.InsertCollection<edm4hep::EventHeader>(std::move(timeslice_info_out),m_config.tag + "ts_info");

        hit_accumulator.clear(); // Reset for next timeslice

        // std::cout << "Number of parents " << child.GetParentNumber(JEventLevel::PhysicsEvent) << std::endl;

        return Result::NextChildNextParent;
    }
};



