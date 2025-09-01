# TimeframeGenerator2 Enhanced Features

This enhanced version of TimeframeGenerator2 now supports additional EDM4HEP and EDM4EIC collection types beyond the original MCParticles.

## New Configuration Options

Add the following options to your `MyTimesliceBuilderConfig`:

```cpp
MyTimesliceBuilderConfig config;

// Enable SimTrackerHits (edm4hep)
config.include_sim_tracker_hits = true;

// Enable ReconstructedParticles with associations (edm4eic) 
config.include_reconstructed_particles = true;
```

## Supported Collection Types

### Original Support
- `edm4hep::MCParticles` → `ts_MCParticles`

### New Support  
- `edm4hep::SimTrackerHits` → `ts_SimTrackerHits`
- `edm4eic::ReconstructedParticles` → `ts_ReconstructedParticles` 
- `edm4eic::MCRecoParticleAssociations` → `ts_MCRecoParticleAssociations`

## Key Features

1. **Consistent Time Offsets**: All collection types (hits, particles, associations) receive the same time offset as calculated from the MCParticles for each event.

2. **Association Maintenance**: MCRecoParticleAssociations are automatically updated to reference the cloned MCParticles in the timeframe rather than the original ones.

3. **Conditional EDM4EIC Support**: The code automatically detects if EDM4EIC is available and enables/disables features accordingly.

## Build Requirements

- EDM4HEP (required)
- EDM4EIC (optional - features disabled if not available)
- JANA2
- PODIO

## Usage Example

```cpp
#include "MyTimesliceBuilder.h"
#include "MyTimesliceBuilderConfig.h"

// Configure the builder
MyTimesliceBuilderConfig config;
config.include_sim_tracker_hits = true;
config.include_reconstructed_particles = true;
config.time_slice_duration = 20.0f;

// Create and use the builder
MyTimesliceBuilder builder(config);
```

The timeframe output will contain all enabled collection types with consistent timing and maintained associations.