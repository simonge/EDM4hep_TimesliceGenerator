# Simplified Collection Processing with C++20

## Overview

This refactoring replaces the previous complex registry system with a simpler C++20-based approach using concepts and compile-time type detection.

## Key Changes

### Before
- Complex `CollectionMergeHandler` registry system (~615 lines)
- Manual handler registration for each collection type
- Lambda-based processing with multiple layers of indirection
- Total: ~920 lines of new code

### After
- Simple `CollectionTypeTraits.h` with C++20 concepts (~129 lines)
- Automatic type detection using `if constexpr`
- Single `applyOffsets()` template function handles all types
- Total: ~276 lines (net reduction of ~644 lines)

## How It Works

### C++20 Concepts
Concepts automatically detect which fields a type has:

```cpp
template<typename T>
concept HasTime = requires(T t) {
    { t.time } -> std::convertible_to<float>;
};

template<typename T>
concept HasGeneratorStatus = requires(T t) {
    { t.generatorStatus } -> std::convertible_to<int>;
};
```

### Compile-Time Processing
The `applyOffsets()` function uses `if constexpr` to automatically apply only the relevant offsets:

```cpp
template<typename T>
void applyOffsets(std::vector<T>& collection, ...) {
    for (auto& item : collection) {
        // Apply time offset only if type has time member
        if constexpr (HasTime<T>) {
            if (!already_merged) {
                item.time += time_offset;
            }
        }
        
        // Apply generator status only if type has it
        if constexpr (HasGeneratorStatus<T>) {
            if (!already_merged) {
                item.generatorStatus += gen_status_offset;
            }
        }
        // ... etc
    }
}
```

### Benefits

1. **Simplicity**: No registries, no handlers, no lambdas
2. **Type Safety**: Compile-time checks ensure correctness
3. **Less Code**: 644 fewer lines than the registry approach
4. **Modern C++**: Uses C++20 concepts and `if constexpr`
5. **Maintainable**: Adding new collection types is straightforward
6. **Performance**: All branching resolved at compile time

## Usage

The simplified approach is transparent to users. Collections are automatically processed based on their type characteristics detected at compile time.
