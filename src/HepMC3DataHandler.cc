#include "HepMC3DataHandler.h"
#include <iostream>
#include <stdexcept>

std::vector<std::unique_ptr<DataSource>> HepMC3DataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {
    
    std::cout << "Initializing HepMC3 data handler for: " << filename << std::endl;
    
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());
    
    // Create HepMC3DataSource objects for each source configuration
    for (size_t source_idx = 0; source_idx < source_configs.size(); ++source_idx) {
        const auto& source_config = source_configs[source_idx];
        
        // Determine format from first input file
        if (source_config.input_files.empty()) {
            throw std::runtime_error("Source " + source_config.name + " has no input files");
        }
        
        const std::string& first_file = source_config.input_files[0];
        
        // Helper lambda to check file extension
        auto hasExtension = [](const std::string& filename, const std::string& ext) {
            if (filename.length() < ext.length()) return false;
            return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
        };
        
        // Check if this is HepMC3 format
        if (!hasExtension(first_file, ".hepmc3.tree.root")) {
            throw std::runtime_error(
                "HepMC3DataHandler can only handle .hepmc3.tree.root files. "
                "Got: " + first_file
            );
        }
        
        auto data_source = std::make_unique<HepMC3DataSource>(source_config, source_idx);
        std::cout << "Created HepMC3DataSource for: " + first_file << std::endl;
        data_sources.push_back(std::move(data_source));
    }
    
    // Store pointers to HepMC3 sources for later use
    hepmc3_sources_.clear();
    hepmc3_sources_.reserve(data_sources.size());
    for (auto& source : data_sources) {
        hepmc3_sources_.push_back(dynamic_cast<HepMC3DataSource*>(source.get()));
    }
    
    // Create HepMC3 writer
    writer_ = std::make_shared<HepMC3::WriterRootTree>(filename);
    if (!writer_) {
        throw std::runtime_error("Failed to create HepMC3 writer for: " + filename);
    }
    
    std::cout << "HepMC3 data handler initialized with " << hepmc3_sources_.size() << " sources" << std::endl;
    
    return data_sources;
}

void HepMC3DataHandler::prepareTimeslice() {
    // Create a new empty event for this timeslice
    current_timeslice_ = std::make_unique<HepMC3::GenEvent>(
        HepMC3::Units::GEV,
        HepMC3::Units::MM
    );
}

void HepMC3DataHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                                      size_t timeslice_number,
                                      float time_slice_duration,
                                      float bunch_crossing_period,
                                      std::mt19937& gen) {
    current_timeslice_number_ = timeslice_number;
    
    // Process each source
    for (size_t source_idx = 0; source_idx < sources.size(); ++source_idx) {
        auto& source = sources[source_idx];
        auto* hepmc3_source = hepmc3_sources_[source_idx];
        
        const auto& config = source->getConfig();
        size_t entries_needed = source->getEntriesNeeded();
        
        // Process each event from this source
        for (size_t entry = 0; entry < entries_needed; ++entry) {
            // Load the next event
            if (!source->loadNextEvent()) {
                std::cerr << "Warning: Failed to load event from source " << config.name << std::endl;
                break;
            }
            
            // Update time offset for this event
            source->UpdateTimeOffset(time_slice_duration, bunch_crossing_period, gen);
            
            // Get the current event from the HepMC3 source
            const auto& event = hepmc3_source->getCurrentEvent();
            
            // Convert time offset to HepMC units (mm using speed of light)
            double time_offset_ns = source->getCurrentTimeOffset();
            
            // Insert the event into the merged timeslice
            long particle_count = insertHepMC3Event(
                event,
                current_timeslice_,
                time_offset_ns,
                config.generator_status_offset
            );
        }
    }
}

long HepMC3DataHandler::insertHepMC3Event(const HepMC3::GenEvent& inevt,
                                            std::unique_ptr<HepMC3::GenEvent>& hepSlice,
                                            double time,
                                            int baseStatus) {
    // Convert time in nanoseconds to HepMC position units (mm)
    double timeHepmc = c_light * time;
    
    std::vector<HepMC3::GenParticlePtr> particles;
    std::vector<HepMC3::GenVertexPtr> vertices;

    // Copy vertices with time offset applied
    for (const auto& vertex : inevt.vertices()) {
        HepMC3::FourVector position = vertex->position();
        position.set_t(position.t() + timeHepmc);
        auto v1 = std::make_shared<HepMC3::GenVertex>(position);
        vertices.push_back(v1);
    }
    
    // Copy particles and attach them to their corresponding vertices
    long finalParticleCount = 0;
    for (const auto& particle : inevt.particles()) {
        HepMC3::FourVector momentum = particle->momentum();
        int status = particle->status();
        if (status == 1) finalParticleCount++;
        int pid = particle->pid();
        status += baseStatus;
        auto p1 = std::make_shared<HepMC3::GenParticle>(momentum, pid, status);
        p1->set_generated_mass(particle->generated_mass());
        particles.push_back(p1);
        
        // Attach particle to production vertex with bounds checking
        if (particle->production_vertex() && particle->production_vertex()->id() < 0) {
            int production_vertex = particle->production_vertex()->id();
            size_t vertex_index = abs(production_vertex) - 1;
            if (vertex_index < vertices.size()) {
                vertices[vertex_index]->add_particle_out(p1);
                hepSlice->add_particle(p1);
            } else {
                std::cerr << "Warning: Invalid production vertex index " << vertex_index 
                          << " (vertices size: " << vertices.size() << ")" << std::endl;
            }
        }
        
        // Attach particle to end vertex if it has one with bounds checking
        if (particle->end_vertex()) {
            int end_vertex = particle->end_vertex()->id();
            size_t vertex_index = abs(end_vertex) - 1;
            if (vertex_index < vertices.size()) {
                vertices[vertex_index]->add_particle_in(p1);
            } else {
                std::cerr << "Warning: Invalid end vertex index " << vertex_index 
                          << " (vertices size: " << vertices.size() << ")" << std::endl;
            }
        }
    }

    // Add all vertices to the merged event
    for (auto& vertex : vertices) {
        hepSlice->add_vertex(vertex);
    }
    
    return finalParticleCount;
}

void HepMC3DataHandler::writeTimeslice() {
    if (!current_timeslice_) {
        throw std::runtime_error("No timeslice to write - prepareTimeslice() not called?");
    }
    
    // Set event number
    current_timeslice_->set_event_number(current_timeslice_number_);
    
    // Write the event
    writer_->write_event(*current_timeslice_);
    
    // Clear the timeslice
    current_timeslice_.reset();
}

void HepMC3DataHandler::finalize() {
    if (writer_) {
        writer_->close();
        writer_.reset();
    }
    std::cout << "HepMC3 output finalized" << std::endl;
}
