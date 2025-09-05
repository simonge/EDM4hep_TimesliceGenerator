#pragma once

#include <string>
#include <vector>

struct MergerConfig {
    bool   introduce_offsets{true};
    float  time_slice_duration{20.0f};
    float  bunch_crossing_period{10.0f};

    // Config per source
    std::vector<struct SourceConfig> sources;

    // Input/output configuration
    std::string output_file{"merged_timeslices.root"};
    size_t max_events{100};
};

struct SourceConfig {
    // Input/output configuration
    std::vector<std::string> input_files;

    bool   already_merged{false};
    bool   static_number_of_events{false};
    size_t static_events_per_timeslice{1};
    float  mean_event_frequency{1.0f};
    bool   use_bunch_crossing{false};

    // Beam background config, beam direction and speed
    bool   attach_to_beam{false};
    float  beam_angle{0.0f};
    float  beam_speed{299792.4580f}; //Speed of light in ns/mm
    float  beam_spread{0.0f};

    // New generator status offset
    int32_t  generator_status_offset{0};
};