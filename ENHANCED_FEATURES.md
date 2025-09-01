# TimeframeGenerator2 Enhanced Features

# TimeframeGenerator2 Enhanced Features

This enhanced version of TimeframeGenerator2 now supports additional EDM4HEP and EDM4EIC collection types beyond the original MCParticles.

## New Architecture

The configuration has been moved to the file reader level for better separation of concerns:

- **MyEventFileReader**: Specifies which collections to read from input files
- **MyTimesliceBuilder**: Generic processor that handles any collection types found in events

## Configuration

Configure which collections to include via the file reader:

```cpp
// In MyEventFileReaderGenerator or during setup:
std::vector<std::string> sim_tracker_hit_collections = {"SimTrackerHits", "OtherHits"};
std::vector<std::string> reconstructed_particle_collections = {"ReconstructedParticles"};

source->SetSimTrackerHitCollections(sim_tracker_hit_collections);
source->SetReconstructedParticleCollections(reconstructed_particle_collections);
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

4. **Generic Processing**: The TimesliceBuilder automatically processes any collections specified in the file reader configuration.

## Build Requirements

- EDM4HEP (required)
- EDM4EIC (optional - features disabled if not available)
- JANA2
- PODIO

## Usage Example

```cpp
#include "MyTimesliceBuilder.h"
#include "MyTimesliceBuilderConfig.h"

// Configure the builder - now collection-agnostic
MyTimesliceBuilderConfig config;
config.time_slice_duration = 20.0f;

// Create the builder - collection selection happens at file reader level
MyTimesliceBuilder builder(config);
```

The timeframe output will contain all collections configured in the file reader with consistent timing and maintained associations.