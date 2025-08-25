#include "CollectionTabulatorsEDM4HEP.h"

JTablePrinter TabulateHitsEDM4HEP(const edm4hep::CalorimeterHitCollection* c) {
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
        t | hit.id() | hit.getCellID() | hit.getEnergy() | hit.getTime() | pos_str.str();
    }
    return t;
}
