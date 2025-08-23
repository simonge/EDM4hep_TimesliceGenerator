#pragma once

#include <edm4hep/ClusterCollection.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include <JANA/Utils/JTablePrinter.h>

JTablePrinter TabulateClusters(const edm4hep::ClusterCollection* c);

JTablePrinter TabulateHits(const edm4hep::CalorimeterHitCollection* c);


