#include "HepMC3TimesliceMerger.h"
#include <iostream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <sys/resource.h>

HepMC3TimesliceMerger::HepMC3TimesliceMerger(const MergerConfig& config)
    : TimesliceMergerBase(config)
{
    std::cout << "=== HepMC3 Timeslice Merger ===" << std::endl;
    std::cout << "Initializing " << m_config.sources.size() << " source(s)" << std::endl;
    
    // Prepare all sources
    for (size_t i = 0; i < m_config.sources.size(); ++i) {
        prepareSource(i);
    }
}

void HepMC3TimesliceMerger::prepareSource(size_t source_idx) {
    if (source_idx >= m_config.sources.size()) {
        throw std::runtime_error("Invalid source index");
    }
    
    const auto& source_config = m_config.sources[source_idx];
    
    if (source_config.input_files.empty()) {
        throw std::runtime_error("Source " + source_config.name + " has no input files");
    }
    
    std::cout << "Preparing source: " << source_config.name << std::endl;
    
    SourceData source_data;
    source_data.config = source_config;
    
    // Open the first file with HepMC3 reader
    // For multiple files, we'll need to handle file chaining
    std::string first_file = source_config.input_files[0];
    
    try {
        source_data.reader = HepMC3::deduce_reader(first_file);
        if (!source_data.reader) {
            throw std::runtime_error("Failed to open HepMC3 file: " + first_file);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error("Error opening " + first_file + ": " + e.what());
    }
    
    // Check if this is a weighted source (frequency <= 0 and not static)
    if (!source_config.static_number_of_events && source_config.mean_event_frequency <= 0) {
        prepareWeightedSource(source_data);
    } else {
        prepareFrequencySource(source_data);
    }
    
    m_sources.push_back(std::move(source_data));
}

void HepMC3TimesliceMerger::prepareFrequencySource(SourceData& source) {
    // Nothing special needed for frequency-based sources
    std::cout << "  Mode: Frequency-based" << std::endl;
    if (source.config.static_number_of_events) {
        std::cout << "  Events per timeslice: " << source.config.static_events_per_timeslice << std::endl;
    } else {
        std::cout << "  Mean frequency: " << source.config.mean_event_frequency << " events/ns" << std::endl;
    }
}

void HepMC3TimesliceMerger::prepareWeightedSource(SourceData& source) {
    std::cout << "  Mode: Weighted (reading all events)" << std::endl;
    
    // Read all events and their weights
    while (!source.reader->failed()) {
        HepMC3::GenEvent evt(HepMC3::Units::GEV, HepMC3::Units::MM);
        source.reader->read_event(evt);
        
        if (source.reader->failed()) break;
        
        double weight = evt.weight();
        if (weight > 0) {
            source.events.push_back(evt);
            source.weights.push_back(weight);
        }
    }
    source.reader->close();
    
    if (source.events.empty()) {
        throw std::runtime_error("No valid weighted events found in source: " + source.config.name);
    }
    
    // Calculate average rate
    source.avg_rate = 0.0;
    for (double w : source.weights) {
        source.avg_rate += w;
    }
    source.avg_rate /= source.weights.size();
    source.avg_rate *= 1e-9; // Convert to 1/ns (GHz)
    
    std::cout << "  Loaded " << source.events.size() << " events" << std::endl;
    std::cout << "  Average rate: " << source.avg_rate << " GHz" << std::endl;
    
    // Create weighted distribution
    std::vector<int> indices(source.weights.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    source.weighted_dist = std::make_unique<std::piecewise_constant_distribution<>>(
        indices.begin(), indices.end(), source.weights.begin()
    );
}

void HepMC3TimesliceMerger::run() {
    auto t_start = std::chrono::high_resolution_clock::now();
    
    printBanner();
    
    // Determine output format from file extension
    bool root_format = (m_config.output_file.find(".root") != std::string::npos);
    
    std::cout << "\nWriting to: " << m_config.output_file << std::endl;
    std::cout << "Format: " << (root_format ? "HepMC3 ROOT" : "HepMC3 ASCII") << std::endl;
    std::cout << "\n==================================================================\n" << std::endl;
    
    // Create output writer
    std::shared_ptr<HepMC3::Writer> writer;
    if (root_format) {
        writer = std::make_shared<HepMC3::WriterRootTree>(m_config.output_file);
    } else {
        writer = std::make_shared<HepMC3::WriterAscii>(m_config.output_file);
    }
    
    // Generate timeslices
    size_t slices_done = 0;
    for (size_t i = 0; i < m_config.max_events; ++i) {
        if (i % 1000 == 0) {
            std::cout << "Processing slice " << i << std::endl;
        }
        
        auto hep_slice = mergeSlice(i);
        if (!hep_slice) {
            std::cout << "Exhausted primary source at slice " << i << std::endl;
            break;
        }
        
        hep_slice->set_event_number(i);
        writer->write_event(*hep_slice);
        slices_done++;
    }
    
    std::cout << "\n==================================================================\n";
    std::cout << "Completed " << slices_done << " timeslices" << std::endl;
    
    // Close all readers
    for (auto& source : m_sources) {
        if (source.reader && !source.reader->failed()) {
            source.reader->close();
        }
    }
    writer->close();
    
    auto t_end = std::chrono::high_resolution_clock::now();
    auto duration_sec = std::chrono::duration<double>(t_end - t_start).count();
    
    std::cout << "\nProcessing time: " << std::round(duration_sec) << " seconds" << std::endl;
    if (slices_done > 0) {
        std::cout << "  --> " << std::round(duration_sec * 1e6 / slices_done) << " us/slice" << std::endl;
    }
    
    printStatistics(slices_done);
}

std::unique_ptr<HepMC3::GenEvent> HepMC3TimesliceMerger::mergeSlice(int slice_idx) {
    auto hep_slice = std::make_unique<HepMC3::GenEvent>(HepMC3::Units::GEV, HepMC3::Units::MM);
    
    // Add events from all sources
    for (auto& source : m_sources) {
        if (source.weighted_dist) {
            addWeightedEvents(source, hep_slice);
        } else {
            addFreqEvents(source, hep_slice);
        }
    }
    
    return hep_slice;
}

void HepMC3TimesliceMerger::addFreqEvents(SourceData& source, std::unique_ptr<HepMC3::GenEvent>& hepSlice) {
    // Generate timeline for events
    std::vector<double> timeline;
    
    if (source.config.static_number_of_events) {
        // Static number of events - place them at random times
        for (size_t i = 0; i < source.config.static_events_per_timeslice; ++i) {
            timeline.push_back(generateRandomTimeOffset());
        }
    } else if (source.config.mean_event_frequency <= 0) {
        // Single event at random time (signal mode)
        timeline.push_back(generateRandomTimeOffset());
    } else {
        // Poisson-distributed times
        timeline = generatePoissonTimes(source.config.mean_event_frequency, m_config.time_slice_duration);
    }
    
    if (timeline.empty()) return;
    
    long particle_count = 0;
    
    // Place events at specified times
    for (double time : timeline) {
        if (source.reader->failed()) {
            if (source.config.repeat_on_eof) {
                // Cycle back to start
                std::cout << "Cycling back to start of " << source.config.name << std::endl;
                source.reader->close();
                source.reader = HepMC3::deduce_reader(source.config.input_files[0]);
                if (!source.reader || source.reader->failed()) {
                    std::cerr << "Warning: Failed to reopen " << source.config.name << std::endl;
                    break;
                }
            } else {
                // Stop if source exhausted and we can't repeat
                break;
            }
        }
        
        HepMC3::GenEvent inevt;
        source.reader->read_event(inevt);
        
        // Check if read failed
        if (source.reader->failed()) {
            // If we can't repeat, just stop
            if (!source.config.repeat_on_eof) {
                break;
            }
            // Otherwise continue to next iteration which will try to reopen
            continue;
        }
        
        // Apply bunch crossing if enabled
        if (source.config.use_bunch_crossing) {
            time = applyBunchCrossing(time);
        }
        
        particle_count += insertHepmcEvent(inevt, hepSlice, time, source.config.generator_status_offset);
    }
    
    source.event_count += timeline.size();
    source.particle_count += particle_count;
}

void HepMC3TimesliceMerger::addWeightedEvents(SourceData& source, std::unique_ptr<HepMC3::GenEvent>& hepSlice) {
    // Determine number of events using Poisson distribution
    int n_events;
    int retry_count = 0;
    const int max_retries = 100;
    while (true) {
        n_events = calculatePoissonEventCount(source.avg_rate, m_config.time_slice_duration);
        if (n_events > static_cast<int>(source.events.size())) {
            retry_count++;
            if (retry_count >= max_retries) {
                std::cout << "WARNING: After " << max_retries << " retries, still trying to place " 
                          << n_events << " events from " << source.config.name 
                          << " but file has only " << source.events.size() 
                          << ". Using available events." << std::endl;
                n_events = source.events.size();
                break;
            }
            continue;
        }
        break;
    }
    
    if (n_events == 0) return;
    
    // Select events using weighted distribution
    std::vector<HepMC3::GenEvent> to_place;
    for (int i = 0; i < n_events; ++i) {
        int idx = static_cast<int>((*source.weighted_dist)(m_rng));
        // Validate index is within bounds
        if (idx >= 0 && idx < static_cast<int>(source.events.size())) {
            to_place.push_back(source.events.at(idx));
        } else {
            std::cerr << "Warning: Invalid event index " << idx << " from weighted distribution" << std::endl;
        }
    }
    
    // Place at random times
    long particle_count = 0;
    
    for (auto& evt : to_place) {
        double time = generateRandomTimeOffset();
        
        // Apply bunch crossing if enabled
        if (source.config.use_bunch_crossing) {
            time = applyBunchCrossing(time);
        }
        
        particle_count += insertHepmcEvent(evt, hepSlice, time, source.config.generator_status_offset);
    }
    
    source.event_count += n_events;
    source.particle_count += particle_count;
}

long HepMC3TimesliceMerger::insertHepmcEvent(const HepMC3::GenEvent& inevt,
                                              std::unique_ptr<HepMC3::GenEvent>& hepSlice,
                                              double time,
                                              int baseStatus) {
    // Convert time to HepMC units (mm)
    double timeHepmc = c_light * time;
    
    std::vector<HepMC3::GenParticlePtr> particles;
    std::vector<HepMC3::GenVertexPtr> vertices;
    
    // Create vertices with time offset
    for (auto& vertex : inevt.vertices()) {
        HepMC3::FourVector position = vertex->position();
        position.set_t(position.t() + timeHepmc);
        auto v1 = std::make_shared<HepMC3::GenVertex>(position);
        vertices.push_back(v1);
    }
    
    // Copy particles and attach to vertices
    long final_particle_count = 0;
    for (auto& particle : inevt.particles()) {
        HepMC3::FourVector momentum = particle->momentum();
        int status = particle->status();
        if (status == 1) final_particle_count++;
        int pid = particle->pid();
        status += baseStatus;
        
        auto p1 = std::make_shared<HepMC3::GenParticle>(momentum, pid, status);
        p1->set_generated_mass(particle->generated_mass());
        particles.push_back(p1);
        
        // Attach to production vertex
        if (particle->production_vertex() && particle->production_vertex()->id() < 0) {
            int production_vertex = particle->production_vertex()->id();
            // Safely convert to size_t, checking for INT_MIN edge case
            if (production_vertex != INT_MIN) {
                int abs_vertex = std::abs(production_vertex);
                if (abs_vertex > 0 && static_cast<size_t>(abs_vertex) <= vertices.size()) {
                    size_t vertex_idx = static_cast<size_t>(abs_vertex) - 1;
                    vertices[vertex_idx]->add_particle_out(p1);
                    hepSlice->add_particle(p1);
                }
            }
        }
        
        // Attach to end vertex
        if (particle->end_vertex()) {
            int end_vertex = particle->end_vertex()->id();
            // Safely convert to size_t, checking for INT_MIN edge case
            if (end_vertex != INT_MIN) {
                int abs_vertex = std::abs(end_vertex);
                if (abs_vertex > 0 && static_cast<size_t>(abs_vertex) <= vertices.size()) {
                    size_t vertex_idx = static_cast<size_t>(abs_vertex) - 1;
                    vertices[vertex_idx]->add_particle_in(p1);
                }
            }
        }
    }
    
    // Add vertices to slice
    for (auto& vertex : vertices) {
        hepSlice->add_vertex(vertex);
    }
    
    return final_particle_count;
}

void HepMC3TimesliceMerger::printBanner() {
    std::cout << "\n==================================================================\n";
    std::cout << "=== HepMC3 Timeslice Merger Configuration ===" << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Number of timeslices: " << m_config.max_events << std::endl;
    std::cout << "Timeslice duration: " << m_config.time_slice_duration << " ns" << std::endl;
    std::cout << "Bunch crossing period: " << m_config.bunch_crossing_period << " ns" << std::endl;
    std::cout << "\nSources:\n";
    
    for (size_t i = 0; i < m_sources.size(); ++i) {
        const auto& source = m_sources[i];
        std::cout << "  [" << i << "] " << source.config.name << std::endl;
        std::cout << "      Files: ";
        for (const auto& file : source.config.input_files) {
            std::cout << file << " ";
        }
        std::cout << std::endl;
        
        if (source.weighted_dist) {
            std::cout << "      Mode: Weighted (avg rate: " << source.avg_rate << " GHz)" << std::endl;
        } else if (source.config.static_number_of_events) {
            std::cout << "      Mode: Static (" << source.config.static_events_per_timeslice 
                      << " events/slice)" << std::endl;
        } else {
            std::cout << "      Mode: Frequency (" << source.config.mean_event_frequency 
                      << " events/ns)" << std::endl;
        }
        
        if (source.config.generator_status_offset != 0) {
            std::cout << "      Status offset: " << source.config.generator_status_offset << std::endl;
        }
    }
    std::cout << "==================================================================\n";
}

void HepMC3TimesliceMerger::printStatistics(int slicesDone) {
    std::cout << "\n=== Statistics ===" << std::endl;
    for (const auto& source : m_sources) {
        std::cout << "Source: " << source.config.name << std::endl;
        std::cout << "  Events placed: " << source.event_count << std::endl;
        if (slicesDone > 0) {
            std::cout << "  Average events/slice: " << std::setprecision(3) 
                      << static_cast<double>(source.event_count) / slicesDone << std::endl;
        }
        std::cout << "  Final state particles: " << source.particle_count << std::endl;
        if (slicesDone > 0) {
            std::cout << "  Average particles/slice: " << std::setprecision(3) 
                      << static_cast<double>(source.particle_count) / slicesDone << std::endl;
        }
    }
    
    // Memory usage
    struct rusage r_usage;
    getrusage(RUSAGE_SELF, &r_usage);
    
#ifdef __MACH__
    float mbsize = 1024 * 1024;
#else
    float mbsize = 1024;
#endif
    
    std::cout << "\nMaximum Resident Memory: " << r_usage.ru_maxrss / mbsize << " MB" << std::endl;
}
