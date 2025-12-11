#pragma once

#include "StandaloneMergerConfig.h"
#include <random>
#include <vector>
#include <memory>

/**
 * @brief Base class for timeslice mergers
 * 
 * Provides common functionality for both EDM4hep and HepMC3 timeslice mergers,
 * including random number generation, Poisson time distribution, and configuration management.
 */
class TimesliceMergerBase {
public:
    /**
     * @brief Constructor
     * @param config Merger configuration
     */
    explicit TimesliceMergerBase(const MergerConfig& config);
    
    /**
     * @brief Virtual destructor
     */
    virtual ~TimesliceMergerBase() = default;
    
    /**
     * @brief Run the merging process (pure virtual)
     */
    virtual void run() = 0;

protected:
    // Configuration
    MergerConfig m_config;
    
    // Random number generation
    std::mt19937 m_rng;
    
    // Speed of light constant (mm/ns) - useful for both formats
    static constexpr double c_light = 299.792458;
    
    /**
     * @brief Generate Poisson-distributed event times within a time window
     * @param mu Mean event frequency (events/ns)
     * @param endTime End of time window (ns)
     * @return Vector of event times
     */
    std::vector<double> generatePoissonTimes(double mu, double endTime);
    
    /**
     * @brief Generate a random time offset within the timeslice duration
     * @return Random time offset in nanoseconds
     */
    double generateRandomTimeOffset();
    
    /**
     * @brief Apply bunch crossing discretization to a time value
     * @param time Input time (ns)
     * @return Discretized time aligned to bunch crossing period
     */
    double applyBunchCrossing(double time) const;
    
    /**
     * @brief Calculate number of events to place using Poisson distribution
     * @param frequency Mean event frequency (events/ns)
     * @param duration Time slice duration (ns)
     * @return Number of events
     */
    size_t calculatePoissonEventCount(double frequency, double duration);
    
    /**
     * @brief Initialize random number generator with seed
     * @param seed Random seed (0 for time-based seed)
     */
    void initializeRNG(unsigned int seed = 0);
};
