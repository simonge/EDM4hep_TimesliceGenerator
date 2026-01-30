# Snakefile template for simulation, merging, and benchmarking

REMOTE_DIR = "root://dtn-eic.jlab.org/"

# Simulation types and their parameters (matching the workflow matrix)
SIMULATIONS = [
    {"type": "signal", "input": "epic_sim_ci_signal.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/DIS/NC/18x275/minQ2=1000/pythia8NCDIS_18x275_minQ2=1000_beamEffects_xAngle=-0.025_hiDiv_1.hepmc3.tree.root"},
    {"type": "minbias", "input": "epic_sim_ci_minbias.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/SIDIS/pythia6-eic/1.0.0/18x275/q2_0to1/pythia_ep_noradcor_18x275_q2_0.000000001_1.0_run1.ab.hepmc3.tree.root"},
    {"type": "hadron_beamgas", "input": "epic_sim_ci_hadron_beamgas.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/BACKGROUNDS/BEAMGAS/proton/pythia8.306-1.0/275GeV/pythia8.306-1.0_ProtonBeamGas_275GeV_run001.hepmc3.tree.root"},
    {"type": "electron_beamgas_bremsstrahlung", "input": "epic_sim_ci_electron_beamgas_brems.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/BACKGROUNDS/BEAMGAS/electron/brems/EIC_ESR_Xsuite/dataprod_rel_1.0.1/18x275/10000Ahr/MachineRuntime50s/dataprod_rel_1.0.1_electron_brems_18x275_10000Ahr.hepmc3.tree.root"},
    {"type": "electron_beamgas_coulomb", "input": "epic_sim_ci_electron_beamgas_coulomb.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/BACKGROUNDS/BEAMGAS/electron/coulomb/EIC_ESR_Xsuite/dataprod_rel_1.0.1/18x275/10000Ahr/MachineRuntime50s/dataprod_rel_1.0.1_electron_coulomb_18x275_10000Ahr.hepmc3.tree.root"},
    {"type": "electron_beamgas_touschek", "input": "epic_sim_ci_electron_beamgas_touschek.edm4hep.root", "events": 10, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/BACKGROUNDS/BEAMGAS/electron/touschek/EIC_ESR_Xsuite/dataprod_rel_1.0.1/18x275/10000Ahr/MachineRuntime50s/dataprod_rel_1.0.1_electron_touschek_18x275_10000Ahr.hepmc3.tree.root"},
    {"type": "electron_synchrotron", "input": "epic_sim_ci_electron_synchrotron.edm4hep.root", "events": 100000, "remote": "root://dtn-eic.jlab.org//volatile/eic/EPIC/EVGEN/BACKGROUNDS/SYNRAD/dataprod_rel_1.0.0/18x275/dataprod_rel_1.0.0_synrad_18x275_run001.hepmc3.tree.root"},
]

# Output files for all simulations
SIM_OUTPUTS = [sim["input"] for sim in SIMULATIONS]

# Merged output file
MERGED_OUTPUT = "merged/merged_timeframes.root"

# Benchmark results
BENCHMARK_OUTPUT = "benchmark/benchmark_results.txt"
rule all:
    input:
        SIM_OUTPUTS

rule simulate_epic:
    output:
        sim_out=lambda wildcards: next(sim["input"] for sim in SIMULATIONS if sim["type"] == wildcards.sim_type)
    params:
        events=lambda wildcards: next(sim["events"] for sim in SIMULATIONS if sim["type"] == wildcards.sim_type),
        input_file=lambda wildcards: next(sim["remote"] for sim in SIMULATIONS if sim["type"] == wildcards.sim_type)
    shell:
        """
        npsim --compactFile $DETECTOR_PATH/epic_craterlake.xml \
              --inputFiles {params.input_file} \
              --outputFile {output.sim_out} \
              --numberOfEvents {params.events}
        """
