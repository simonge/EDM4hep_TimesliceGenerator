# Snakefile

import os

# This sets a default config file, but Snakemake will override it if --configfile is passed
configfile: "config.yml"

# Setup config
input_files           = [entry["input_file"] for entry in config["inputs"]]
n_events              = config["n_events"]
timeslice_duration    = config["timeslice_duration"]
bunch_crossing_period = config["bunch_crossing_period"]
merged_file           = config["output_file"]

# Generate jana args for each input file based on config

def build_jana_args(entry):
    args = []
    if "use_bunch_crossing" in entry:
        args.append(f"-Ptimeslice:use_bunch_crossing={entry['use_bunch_crossing']}")
    if "static_number_of_events" in entry:
        args.append(f"-Ptimeslice:static_number_of_events={entry['static_number_of_events']}")
        if "static_events_per_timeslice" in entry:
            args.append(f"-Ptimeslice:static_events_per_timeslice={entry['static_events_per_timeslice']}")
    if "mean_event_frequency" in entry:
        args.append(f"-Ptimeslice:mean_event_frequency={entry['mean_event_frequency']}")
    if "attach_to_beam" in entry:
        args.append(f"-Ptimeslice:attach_to_beam={entry['attach_to_beam']}")
        if "beam_speed" in entry:
            args.append(f"-Ptimeslice:beam_speed={entry['beam_speed']}")
        if "beam_spread" in entry:
            args.append(f"-Ptimeslice:beam_spread={entry['beam_spread']}")
    if "generator_status_offset" in entry:
        args.append(f"-Ptimeslice:generator_status_offset={entry['generator_status_offset']}")
    return " ".join(args)

jana_args_dict = {entry["input_file"]: build_jana_args(entry) for entry in config["inputs"]}

# Create array of temporary output file names - strip extension and add .timeslice.root
def timeslice_output_name(input_file):
    base, ext = os.path.splitext(input_file)
    return f"{base}.timeslice{ext}"

output_files = [timeslice_output_name(f) for f in input_files]

rule all:
    input:
        merged_file

rule run_timeslice_creator:
    input:
        infile = "{basename}.{ext}"
    output:
        outfile = "{basename}.timeslice.{ext}"
    params:
        # Add any extra parameters needed for your jana command
        jana_args = lambda wildcards: jana_args_dict[f"{wildcards.basename}.{wildcards.ext}"],
        n_events = n_events,
        timeslice_duration = timeslice_duration,
        bunch_crossing_period = bunch_crossing_period
    shell:
        """jana -Pplugins=TimesliceCreator \
             -Pwriter:nevents={params.n_events} \
             -Ptimeslice:duration={params.timeslice_duration} \
             -Ptimeslice:bunch_crossing_period={params.bunch_crossing_period} \
             {params.jana_args} \
             -Poutput_file={output.outfile} \
             {input.infile}"""

rule merge_podio_files:
    input:
        output_files
    output:
        merged_file
    shell:
        """
        podio-merge-files --output-file {output} {input} 
        """