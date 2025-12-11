#include "TimesliceMergerBase.h"
#include <chrono>
#include <cmath>
#include <random>

TimesliceMergerBase::TimesliceMergerBase(const MergerConfig& config)
    : m_config(config)
{
    // Initialize RNG with time-based seed
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    m_rng.seed(seed);
}

void TimesliceMergerBase::initializeRNG(unsigned int seed) {
    if (seed == 0) {
        // Use time-based seed
        seed = std::chrono::system_clock::now().time_since_epoch().count();
    }
    m_rng.seed(seed);
}

std::vector<double> TimesliceMergerBase::generatePoissonTimes(double mu, double endTime) {
    std::exponential_distribution<> exp(mu);
    
    double t = 0;
    std::vector<double> ret;
    while (true) {
        double delt = exp(m_rng); // mu is in events/ns, so delt is in ns
        t += delt;
        if (t >= endTime) {
            break;
        }
        ret.push_back(t);
    }
    return ret;
}

double TimesliceMergerBase::generateRandomTimeOffset() {
    std::uniform_real_distribution<> uni(0, m_config.time_slice_duration);
    return uni(m_rng);
}

double TimesliceMergerBase::applyBunchCrossing(double time) const {
    return std::floor(time / m_config.bunch_crossing_period) * m_config.bunch_crossing_period;
}

size_t TimesliceMergerBase::calculatePoissonEventCount(double frequency, double duration) {
    std::poisson_distribution<> poisson_dist(duration * frequency);
    return poisson_dist(m_rng);
}
