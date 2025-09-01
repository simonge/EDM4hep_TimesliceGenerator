
#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

// Forward declaration to avoid JANA dependency in simple tests
#ifndef JANA_JEVENTLEVEL_H
enum class JEventLevel { PhysicsEvent };
#endif

struct MyTimesliceBuilderConfig {
    std::string tag{"det1"};
    JEventLevel parent_level{JEventLevel::PhysicsEvent};
    float  time_slice_duration{20.0f};
    bool   static_number_of_hits{false};
    size_t static_hits_per_timeslice{1};
    float  mean_hit_frequency{1.0f};
    float  bunch_crossing_period{10.0f};
    bool   use_bunch_crossing{false};

    // Beam background config, beam direction and speed
    bool   attach_to_beam{false};
    float  beam_angle{0.0f};
    float  beam_speed{299792.4580f}; //Speed of light in ns/mm
    float  beam_spread{0.0f};

    // New generator status offset
    int32_t  generator_status_offset{0};

    // Collection configuration - lists of collections to accumulate
    std::vector<std::string> sim_tracker_hit_collections;
    std::vector<std::string> reconstructed_particle_collections;
    std::vector<std::string> vertex_collections;  // For timing information

};
