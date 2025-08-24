#pragma once

#include <edm4hep/ClusterCollection.h>
#include <edm4hep/CalorimeterHitCollection.h>
#include <JANA/Utils/JTablePrinter.h>

JTablePrinter TabulateClustersEDM4HEP(const edm4hep::ClusterCollection* c);

JTablePrinter TabulateHitsEDM4HEP(const edm4hep::CalorimeterHitCollection* c);
