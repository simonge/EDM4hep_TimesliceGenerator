# PodioCollectionZipReader - Enhanced Collection Processing

This document describes the new `PodioCollectionZipReader` class that provides enhanced collection processing capabilities for the TimeframeGenerator2 project.

## Features

### 1. Collection Zipping
The `PodioCollectionZipReader` allows you to "zip" multiple collections together for coordinated iteration:

```cpp
// Example: Process particles and hits together
auto frame = zip_reader->readEntry("events", 0);
std::vector<std::string> collections = {"MCParticles", "TrackerHits", "CaloHits"};
auto zipped = zip_reader->zipCollections(*frame, collections);

// Iterate over all collections simultaneously
for (auto it = zipped.begin(); it != zipped.end(); ++it) {
    auto particle = it.getElement<edm4hep::MCParticleCollection>("MCParticles");
    auto tracker_hit = it.getElement<edm4hep::SimTrackerHitCollection>("TrackerHits");
    auto calo_hit = it.getElement<edm4hep::SimCalorimeterHitCollection>("CaloHits");
    
    // Process related data together
    processRelatedData(particle, tracker_hit, calo_hit);
}
```

### 2. Vectorized Time Operations
Efficient batch processing of time offsets across entire collections:

```cpp
// Apply time offset to all particles at once
PodioCollectionZipReader::addTimeOffsetVectorized(particles, 10.0f);

// Apply to all hits
PodioCollectionZipReader::addTimeOffsetVectorized(tracker_hits, 10.0f);
PodioCollectionZipReader::addTimeOffsetVectorized(calo_hits, 10.0f);

// Apply to entire frame (all time-bearing collections)
PodioCollectionZipReader::addTimeOffsetToFrame(*frame, 10.0f);
```

### 3. Type-Safe Collection Access
Find collections by their EDM4HEP type:

```cpp
auto mc_collections = PodioCollectionZipReader::getCollectionNamesByType(*frame, "edm4hep::MCParticle");
auto tracker_collections = PodioCollectionZipReader::getCollectionNamesByType(*frame, "edm4hep::SimTrackerHit");
```

## Integration with StandaloneTimesliceMerger

The new reader is integrated into the existing merger through the `mergeCollectionsWithZipping` method, which:

1. Uses vectorized time offset operations for better performance
2. Maintains all existing functionality and compatibility
3. Provides cleaner, more maintainable code

## Benefits

1. **Performance**: Vectorized operations are more efficient than element-by-element processing
2. **Maintainability**: Cleaner separation of concerns with dedicated collection processing methods
3. **Flexibility**: Collection zipping allows for more sophisticated processing patterns
4. **Compatibility**: Existing input/output formats remain unchanged

## Usage Example

```cpp
// Create zip reader
auto zip_reader = std::make_shared<PodioCollectionZipReader>(input_files);

// Read frame
auto frame = zip_reader->readEntry("events", 0);

// Apply vectorized time offset
PodioCollectionZipReader::addTimeOffsetToFrame(*frame, time_offset);

// Process collections with zipping for coordinated access
std::vector<std::string> collections_to_zip = {"MCParticles", "TrackerHits"};
auto zipped = zip_reader->zipCollections(*frame, collections_to_zip);

// Use functional-style iteration
zip_reader->forEachZipped(zipped, [](const auto& it) {
    // Process elements at the same index across collections
    auto particle = it.getElement<edm4hep::MCParticleCollection>("MCParticles");
    auto hit = it.getElement<edm4hep::SimTrackerHitCollection>("TrackerHits");
    // ... processing logic
});
```

This enhancement maintains full backward compatibility while providing powerful new capabilities for efficient collection processing.