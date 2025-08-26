#pragma once

#include <edm4hep/SimTrackerHitCollection.h>
#include <JANA/Utils/JTablePrinter.h>


JTablePrinter TabulateHitsEDM4HEP(const edm4hep::SimTrackerHitCollection* c);
