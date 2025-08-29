#include "CollectionTabulatorsEDM4HEP.h"

JTablePrinter TabulateHitsEDM4HEP(const edm4hep::SimTrackerHitCollection* c) {
    JTablePrinter t;
    t.AddColumn("hitId");
    t.AddColumn("cellId");
    t.AddColumn("energy");
    t.AddColumn("time");
    t.AddColumn("position");
    for (auto hit : *c) {
        auto pos = hit.getPosition();
        std::ostringstream pos_str;
        pos_str << "(" << pos.x << "," << pos.y << "," << pos.z << ")";
        t | hit.id() | hit.getCellID() | hit.getEDep() | hit.getTime() | pos_str.str();
    }
    return t;
}

// JTablePrinter TabulateParticlesEDM4HEP(const edm4hep::MCParticleCollection* c) {
//     JTablePrinter t;
//     t.AddColumn("particleId");
//     t.AddColumn("PDG");
//     t.AddColumn("mass");
//     t.AddColumn("charge");
//     t.AddColumn("time");
//     t.AddColumn("position");
//     for (auto particle : *c) {
//         auto pos = particle.getPosition();
//         std::ostringstream pos_str;
//         pos_str << "(" << pos.x << "," << pos.y << "," << pos.z << ")";
//         t | particle.id() | particle.getPDG() | particle.getMass() | particle.getCharge() | particle.getTime() | pos_str.str();
//     }
