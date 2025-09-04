
#pragma once

struct MyTimesliceBuilderConfig {
    JEventLevel parent_level{JEventLevel::PhysicsEvent};
    float  time_slice_duration{20.0f}; // ns
    bool   static_number_of_events{false};
    size_t static_events_per_timeslice{1};
    float  mean_event_frequency{1.0f}; // 1/ns
    float  bunch_crossing_period{10.0f}; // ns
    bool   use_bunch_crossing{false};

    // Beam background config, beam direction and speed
    bool   attach_to_beam{false};
    float  beam_angle{0.0f};
    float  beam_speed{299792.4580f}; //Speed of light in ns/mm
    float  beam_spread{0.0f}; // Gausissian std dev ns

    // Offset clock
    bool   apply_clock_offset{false};
    bool   clock_offset_is_random{true};
    float  static_clock_offset{0.0f}; // ns

    // New generator status offset
    int32_t  generator_status_offset{0};

};
