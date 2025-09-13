# Object-Oriented Refactoring Summary

## Overview
This document summarizes the object-oriented refactoring of the `StandaloneTimesliceMerger` from a procedural approach to a more object-oriented design pattern.

## Key Architectural Changes

### 1. Introduction of DataSource Class
**Before:** Used a simple `SourceReader` struct containing only data members and no methods.
**After:** Created a comprehensive `DataSource` class that encapsulates:
- Data management (branch pointers, entry tracking)
- Initialization logic (file loading, branch setup)
- Event processing methods (data merging, time offset calculation)
- Utility methods (status reporting, validation)

### 2. Separation of Concerns
**Before:** `StandaloneTimesliceMerger` handled all aspects of data source management.
**After:** Clear separation between:
- `StandaloneTimesliceMerger`: Orchestrates the overall merging process
- `DataSource`: Manages individual source-specific operations

### 3. Method Extraction and Encapsulation
**Moved from StandaloneTimesliceMerger to DataSource:**
- `initializeInputFiles()` → `DataSource::initialize()`
- `mergeEventData()` → `DataSource::mergeEventData()`
- `generateTimeOffset()` → `DataSource::generateTimeOffset()`
- Branch setup logic → `DataSource::setupBranches()` and helper methods

## Code Quality Improvements

### 1. Naming Conventions
- **Private members**: Use trailing underscore (e.g., `config_`, `source_index_`)
- **Method names**: Descriptive and action-oriented (e.g., `hasMoreEntries()`, `printStatus()`)
- **Variables**: Clear, self-documenting names

### 2. Encapsulation
- **Private data**: All internal state is private with controlled public access
- **Helper methods**: Complex operations broken into smaller, focused private methods
- **Const-correctness**: Methods that don't modify state are marked `const`

### 3. Memory Management
- **RAII**: Automatic cleanup in destructor
- **Smart pointers**: Use `std::unique_ptr` for resource management
- **Resource cleanup**: Dedicated `cleanup()` method for proper deallocation

### 4. Modularity
**Before:** Single large method handling all event merging logic
**After:** Focused methods for specific tasks:
- `calculateBeamDistance()`: Beam physics calculations
- `updateParticleReferences()`: MCParticle reference management
- `updateTrackerHitData()`: Tracker hit processing
- `setupMCParticleBranches()`, `setupTrackerBranches()`, etc.: Specialized setup

## Interface Design

### DataSource Public Interface
```cpp
class DataSource {
public:
    // Construction and initialization
    DataSource(const SourceConfig& config, size_t source_index);
    void initialize(const std::vector<std::string>& collections...);
    
    // Data access and state management
    bool hasMoreEntries() const;
    size_t getTotalEntries() const;
    void setEntriesNeeded(size_t entries);
    
    // Core functionality
    void mergeEventData(...);
    float generateTimeOffset(...);
    
    // Utility and diagnostics
    void printStatus() const;
    bool isInitialized() const;
};
```

### StandaloneTimesliceMerger Changes
```cpp
class StandaloneTimesliceMerger {
private:
    // Object-oriented design with owned DataSource objects
    std::vector<std::unique_ptr<DataSource>> data_sources_;
    
    // Cleaner method signatures
    std::vector<std::unique_ptr<DataSource>> initializeDataSources();
    bool updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources);
    void createMergedTimeslice(std::vector<std::unique_ptr<DataSource>>& sources, ...);
};
```

## Benefits of the Refactoring

### 1. Maintainability
- **Single responsibility**: Each class has a clear, focused purpose
- **Easier debugging**: Isolated functionality easier to trace and fix
- **Code reuse**: DataSource can potentially be reused in other contexts

### 2. Extensibility
- **New data types**: Easy to add by extending DataSource methods
- **Different sources**: New source types can inherit from or compose with DataSource
- **Additional functionality**: Clean interfaces for adding features

### 3. Testability
- **Unit testing**: Individual DataSource operations can be tested in isolation
- **Mock objects**: Interfaces support dependency injection for testing
- **State validation**: Clear methods for checking object state

### 4. Readability
- **Self-documenting**: Method names clearly indicate functionality
- **Logical grouping**: Related functionality grouped in appropriate classes
- **Reduced complexity**: Smaller, focused methods easier to understand

## Preserved Functionality

The refactoring maintains 100% compatibility with:
- **Configuration system**: No changes to `MergerConfig` or `SourceConfig`
- **Command-line interface**: All existing options work identically
- **File formats**: Input and output formats unchanged
- **Physics behavior**: Identical time offset calculation and merging logic
- **Error handling**: Same error conditions and messages

## Migration Impact

**For Users:** No changes required - same command line interface and configuration files
**For Developers:** Improved code structure with better separation of concerns and cleaner interfaces for future modifications

## Future Enhancement Opportunities

The new object-oriented structure enables:
1. **Plugin architecture**: Easy to add new data source types
2. **Parallel processing**: DataSource objects can be processed independently
3. **Caching strategies**: Per-source caching and optimization
4. **Advanced diagnostics**: Rich per-source monitoring and reporting
5. **Configuration validation**: Better error checking and user feedback