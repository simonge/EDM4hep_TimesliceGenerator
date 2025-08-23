// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include <edm4hep/ClusterCollection.h>


struct MyProtoclusterFactory : public JOmniFactory<MyProtoclusterFactory> {

    PodioInput<edm4hep::CalorimeterHit> hits_in {this};
    PodioOutput<edm4hep::Cluster> clusters_out {this};

    void Configure() {
    }

    void ChangeRun(int32_t /*run_nr*/) {
    }

    void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {

        auto cs = std::make_unique<edm4hep::ClusterCollection>();
        for (auto hit : *hits_in()) {
            auto cluster = edm4hep::MutableCluster();
            cluster.setEnergy(hit.getEnergy());
            cluster.setPosition(hit.getPosition());
            cluster.setType(1); // Set a default cluster type
            // Note: edm4hep::Cluster doesn't have addHits method like ExampleCluster
            // You may need to use a different approach for hit-cluster associations
            cs->push_back(cluster);
        }
        clusters_out() = std::move(cs);
    }
};


