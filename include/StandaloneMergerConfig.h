#pragma once

#include <string>
#include <vector>

class SourceConfig {
public:
    // Constructor with default values
    SourceConfig() = default;
    
    // Getters
    const std::vector<std::string>& getInputFiles() const { return input_files_; }
    const std::string& getName() const { return name_; }
    bool isAlreadyMerged() const { return already_merged_; }
    bool useStaticNumberOfEvents() const { return static_number_of_events_; }
    size_t getStaticEventsPerTimeslice() const { return static_events_per_timeslice_; }
    float getMeanEventFrequency() const { return mean_event_frequency_; }
    bool useBunchCrossing() const { return use_bunch_crossing_; }
    bool attachToBeam() const { return attach_to_beam_; }
    float getBeamAngle() const { return beam_angle_; }
    float getBeamSpeed() const { return beam_speed_; }
    float getBeamSpread() const { return beam_spread_; }
    int32_t getGeneratorStatusOffset() const { return generator_status_offset_; }
    const std::string& getTreeName() const { return tree_name_; }
    
    // Setters
    void setInputFiles(const std::vector<std::string>& files) { input_files_ = files; }
    void addInputFile(const std::string& file) { input_files_.push_back(file); }
    void setName(const std::string& name) { name_ = name; }
    void setAlreadyMerged(bool merged) { already_merged_ = merged; }
    void setStaticNumberOfEvents(bool static_events) { static_number_of_events_ = static_events; }
    void setStaticEventsPerTimeslice(size_t events) { static_events_per_timeslice_ = events; }
    void setMeanEventFrequency(float frequency) { mean_event_frequency_ = frequency; }
    void setUseBunchCrossing(bool use_crossing) { use_bunch_crossing_ = use_crossing; }
    void setAttachToBeam(bool attach) { attach_to_beam_ = attach; }
    void setBeamAngle(float angle) { beam_angle_ = angle; }
    void setBeamSpeed(float speed) { beam_speed_ = speed; }
    void setBeamSpread(float spread) { beam_spread_ = spread; }
    void setGeneratorStatusOffset(int32_t offset) { generator_status_offset_ = offset; }
    void setTreeName(const std::string& name) { tree_name_ = name; }
    
    // Validation methods
    bool hasInputFiles() const { return !input_files_.empty(); }
    bool isValid() const { 
        return hasInputFiles() && !name_.empty() && 
               static_events_per_timeslice_ > 0 && 
               mean_event_frequency_ > 0.0f && 
               beam_speed_ > 0.0f;
    }

private:
    // Input/output configuration
    std::vector<std::string> input_files_;
    std::string name_{"signal"};

    bool   already_merged_{false};
    bool   static_number_of_events_{false};
    size_t static_events_per_timeslice_{1};
    float  mean_event_frequency_{1.0f};
    bool   use_bunch_crossing_{false};

    // Beam background config, beam direction and speed
    bool   attach_to_beam_{false};
    float  beam_angle_{0.0f};
    float  beam_speed_{299792.4580f}; //Speed of light in ns/mm
    float  beam_spread_{0.0f};

    // New generator status offset
    int32_t  generator_status_offset_{0};
    std::string tree_name_{"events"};
};

struct MergerConfig {
    bool   introduce_offsets{true};
    float  time_slice_duration{20.0f};
    float  bunch_crossing_period{10.0f};

    // Config per source
    std::vector<SourceConfig> sources;

    // Input/output configuration
    std::string output_file{"merged_timeslices.root"};
    size_t max_events{100};
    bool   merge_particles{false};
};