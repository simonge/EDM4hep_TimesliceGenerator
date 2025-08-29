// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright (C) 2024 Simon Gardner

#pragma once

#include <JANA/Components/JOmniFactory.h>
#include <edm4hep/SimTrackerHitCollection.h>

struct SimTrackerHitCollector_factory : public JOmniFactory<SimTrackerHitCollector_factory> {

  VariadicPodioInput<edm4hep::SimTrackerHit>  m_inputs{this, {.is_optional = true}};
  PodioOutput<edm4hep::SimTrackerHit> m_output{this};

  SimTrackerHitCollector_factory() {
    SetTypeName(NAME_OF_THIS);
    std::cout << "Output databundle name: "
                          << m_output.GetDatabundle()->GetShortName()
                          << std::endl;
  }

  void Configure() {}

  void ChangeRun(int32_t /*run_nr*/) {
  }

  void Execute(int32_t /*run_nr*/, uint64_t /*evt_nr*/) {

    auto output = std::make_unique<edm4hep::SimTrackerHitCollection>();
    // output->setSubsetCollection();

    // for (const auto& hit : *m_inputs()) {
    //   output->push_back(hit.clone(true));
    // }

    for (const auto& in_collection : m_inputs()) {

      // Check if input collection exists
      if (!in_collection) {
        std::cerr << "ERROR: Input collection not found!" << std::endl;
        continue;
      }

      for (const auto& hit : *in_collection) {
        output->push_back(hit.clone(true));
      }
    }

    m_output() = std::move(output);
    //Print the name of the output databundle to verify it is correctly set

  }
  
};
