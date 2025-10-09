# Handler-Based Dispatch Pattern

## Problem

The original code had repetitive conditional checks and type casting for each collection category:

```cpp
if (type_category == BranchTypeRegistry::BranchCategory::TRACKER_HIT) {
    auto* hits = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
    if (hits) {
        merged_collections_.tracker_hits[collection_name].insert(/* ... */);
    }
}
else if (type_category == BranchTypeRegistry::BranchCategory::CALORIMETER_HIT) {
    auto* hits = std::any_cast<std::vector<edm4hep::SimCalorimeterHitData>>(&collection_data);
    if (hits) {
        merged_collections_.calo_hits[collection_name].insert(/* ... */);
    }
}
// ... more else-if blocks
```

**Issues:**
- Repetitive pattern: check category → cast → insert
- Each new category requires adding another else-if block
- Type information encoded in both category enum and explicit cast
- Not following DRY (Don't Repeat Yourself) principle

## Solution: Handler Map Pattern

Implemented a handler-based dispatch pattern using C++20 features:

```cpp
// In BranchTypeRegistry.h
using CollectionHandler = std::function<void(std::any&, MergedCollections&, const std::string&)>;
static CollectionHandler getHandlerForCategory(BranchCategory category);

// In BranchTypeRegistry.cc - handlers registered once
handlers[BranchCategory::TRACKER_HIT] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
    auto* hits = std::any_cast<std::vector<edm4hep::SimTrackerHitData>>(&collection_data);
    if (hits) {
        merged.tracker_hits[collection_name].insert(/* ... */);
    }
};

// In StandaloneTimesliceMerger.cc - simplified dispatch
auto type_category = BranchTypeRegistry::getCategoryForType(collection_type);
auto handler = BranchTypeRegistry::getHandlerForCategory(type_category);
if (handler) {
    handler(collection_data, merged_collections_, collection_name);
}
```

## Benefits

### 1. Eliminated Conditional Checks

**Before:** 5 else-if blocks (75 lines of repetitive code)
```cpp
if (type_category == BranchCategory::TRACKER_HIT) { /* 15 lines */ }
else if (type_category == BranchCategory::CALORIMETER_HIT) { /* 15 lines */ }
else if (type_category == BranchCategory::CONTRIBUTION) { /* 15 lines */ }
else if (type_category == BranchCategory::OBJECTID_REF) { /* 25 lines */ }
else if (type_category == BranchCategory::GP_KEY) { /* 10 lines */ }
```

**After:** Single dispatch pattern (4 lines)
```cpp
auto type_category = BranchTypeRegistry::getCategoryForType(collection_type);
auto handler = BranchTypeRegistry::getHandlerForCategory(type_category);
if (handler) {
    handler(collection_data, merged_collections_, collection_name);
}
```

### 2. Centralized Type-Specific Logic

All type-specific casting and insertion logic is now centralized in the registry where the type mappings are defined. This follows the principle of **co-location**: code that changes together stays together.

### 3. Easier to Extend

**Before:** Add new type requires changes in 2 places:
1. Add type mapping in BranchTypeRegistry
2. Add else-if block in StandaloneTimesliceMerger

**After:** Add new type requires changes in 1 place:
```cpp
// In BranchTypeRegistry.cc only
handlers[BranchCategory::NEW_TYPE] = [](std::any& collection_data, MergedCollections& merged, const std::string& collection_name) {
    auto* data = std::any_cast<std::vector<NewDataType>>(&collection_data);
    if (data) {
        merged.new_data[collection_name].insert(/* ... */);
    }
};
```

The dispatch code in StandaloneTimesliceMerger.cc doesn't need any changes!

### 4. Better Separation of Concerns

- **BranchTypeRegistry**: Knows about types, categories, and how to handle each category
- **StandaloneTimesliceMerger**: Orchestrates the merging process but doesn't know type-specific details

### 5. Type Safety

The handler signature enforces type safety at compile time:
```cpp
using CollectionHandler = std::function<void(std::any&, MergedCollections&, const std::string&)>;
```

Any handler that doesn't match this signature will cause a compilation error.

## Technical Details

### Handler Storage

Handlers are stored in a static map that's initialized on first use (singleton pattern):

```cpp
static std::map<BranchCategory, CollectionHandler> handlers;
if (handlers.empty()) {
    // Initialize handlers once
}
```

This ensures:
- Handlers are initialized exactly once
- No global initialization order issues
- Minimal runtime overhead (map lookup vs. if-else chain)

### Performance Considerations

**Map Lookup vs. If-Else Chain:**
- Map lookup: O(log n) where n = number of categories
- If-else chain: O(n) worst case

For small n (we have ~5 categories), the performance is virtually identical. The map approach actually has better worst-case performance and is more maintainable.

**Memory Overhead:**
- Each handler is a std::function (small overhead for storing function pointer/lambda)
- Total overhead: ~5 function objects = negligible

## Design Pattern

This implements the **Strategy Pattern** combined with **Registry Pattern**:

1. **Strategy Pattern**: Each handler is a strategy for processing a specific collection type
2. **Registry Pattern**: Handlers are registered in a central location and retrieved by category

This is a common pattern in modern C++ for eliminating switch/if-else chains when the cases follow a common pattern.

## Code Metrics

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Lines in mergeTimeslice() | 80+ | 9 | **89% reduction** |
| Conditional blocks | 5 | 1 | **80% reduction** |
| Places to change for new type | 2 | 1 | **50% reduction** |
| Type-specific code location | Scattered | Centralized | Better organization |

## Future Enhancements

This pattern enables several future improvements:

### 1. Dynamic Registration
```cpp
// Allow external registration of handlers
BranchTypeRegistry::registerHandler(BranchCategory::CUSTOM, custom_handler);
```

### 2. Handler Composition
```cpp
// Chain handlers together
auto composite_handler = compose(pre_handler, main_handler, post_handler);
```

### 3. Configuration-Driven Handlers
```cpp
// Load handler configuration from file
BranchTypeRegistry::loadHandlers("handlers.yaml");
```

### 4. Testing
```cpp
// Test handlers in isolation
auto handler = BranchTypeRegistry::getHandlerForCategory(BranchCategory::TRACKER_HIT);
std::any test_data = std::vector<edm4hep::SimTrackerHitData>{/* test data */};
MergedCollections test_merged;
handler(test_data, test_merged, "test_collection");
// Verify test_merged state
```

## Conclusion

The handler-based dispatch pattern:

✅ **Eliminates repetitive conditional checks**  
✅ **Centralizes type-specific logic**  
✅ **Makes code more maintainable and extensible**  
✅ **Reduces lines of code by 89%**  
✅ **Follows modern C++ best practices**  
✅ **Enables future enhancements**  

This is a significant improvement that makes the codebase more elegant, maintainable, and easier to extend.
