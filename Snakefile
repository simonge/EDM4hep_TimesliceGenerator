# Snakefile

# Set default value
default_input_files = ["input.root", "input2.root"]


# List your input files (or parameters for each TimesliceCreator run)

# Use config value if provided, else use default
input_files = config.get("input_files", default_input_files)

output_files = ["timeslice_{idx}.root".format(idx=i+1) for i in range(len(input_files))]
merged_file = "merged_timeslices.root"

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
        jana_args = ""
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