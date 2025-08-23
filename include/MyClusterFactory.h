// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.

#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <edm4hep/ClusterCollection.h>


struct MyClusterFactory : public JOmniFactory<MyClusterFactory> {

    PodioInput<edm4hep::Cluster> m_protoclusters_in {this};
    PodioOutput<edm4hep::Cluster> m_clusters_out {this};


    void Configure() {
    }

    void ChangeRun(int32_t /*run_nr*/) {
    }

    void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {

        auto cs = std::make_unique<edm4hep::ClusterCollection>();

        for (auto protocluster : *m_protoclusters_in()) {
            auto cluster = edm4hep::MutableCluster();
            cluster.setEnergy(protocluster.getEnergy() + 1000);
            cluster.setPosition(protocluster.getPosition());
            cluster.setType(protocluster.getType());
            // Note: edm4hep::Cluster doesn't have addClusters method like ExampleCluster
            // You may need to handle cluster associations differently
            cs->push_back(cluster);
        }

        m_clusters_out() = std::move(cs);
    }
};


