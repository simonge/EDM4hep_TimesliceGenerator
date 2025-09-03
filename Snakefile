# Snakefile

# Set default value
default_input_files = ["input.root", "input2.root"]
default_output_file = "merged_timeslices.root"
default_timeslice_duration = 100.0 # ns
default_bunch_crossing_period = 10.0 # ns


# List your input files (or parameters for each TimesliceCreator run)

# Use config value if provided, else use default
input_files           = config.get("input_files", default_input_files)
output_files          = ["timeslice_{idx}.root".format(idx=i+1) for i in range(len(input_files))]
merged_file           = config.get("output_file", default_output_file)
timeslice_duration    = config.get("timeslice_duration", default_timeslice_duration)
bunch_crossing_period = config.get("bunch_crossing_period", default_bunch_crossing_period)



rule all:
    input:
        merged_file

rule run_timeslice_creator:
    input:
        infile = lambda wildcards: input_files[int(wildcards.idx)-1]
    output:
        outfile="timeslice_{idx}.root"
    params:
        # Add any extra parameters needed for your jana command
        jana_args = "-Ptimeslice:duration={}".format(timeslice_duration) +
                     "-Ptimeslice:bunch_crossing_period={}".format(bunch_crossing_period)
    shell:
        """
        jana -Pplugins=TimesliceCreator -Pwriter:nevents=100  -Poutput_file={output.outfile} {params.jana_args} {input.infile}
        """

rule merge_podio_files:
    input:
        output_files
    output:
        merged_file
    shell:
        """
        podio-merge-files --output-file {output} {input} 
        """