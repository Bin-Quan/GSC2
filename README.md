# GSC (Genotype Sparse Compression)
Genotype Sparse Compression (GSC) is an advanced tool for lossless compression of VCF files, designed to efficiently store and manage VCF files in a compressed format. It accepts VCF/BCF files as input and utilizes advanced compression techniques to significantly reduce storage requirements while ensuring fast query capabilities. In our study, we successfully compressed the VCF files from the 1000 Genomes Project (1000Gpip3), consisting of 2504 samples and 80 million variants, from an uncompressed VCF file of 803.70GB to approximately 1GB.

## Requirements 
### GSC requires:

- **Compiler Compatibility**: GSC requires a modern C++14-ready compiler, such as:
  - g++ version 10.1.0 or higher

- **Build System**: Make build system is necessary for compiling GSC.

- **Operating System**: GSC supports 64-bit operating systems, including:
  - Linux (Ubuntu 18.04)

## build
### Dockerfile
Dockerfile can be used to build a Docker image with all necessary dependencies and GSC compressor. The image is based on Ubuntu 18.04. To build a Docker image and run a Docker container, you need Docker Desktop (https://www.docker.com). Example commands (run it within a directory with Dockerfile):
```bash
#build
docker build -t gsc_project .

#run
docker run -it gsc_project
```
### Building the GSC command line tool

```bash
#Clone the repository
git clone https://github.com/luo-xiaolong/GSC.git
cd GSC

# Clean the previous GSC build 
make clean

# Build the application
make
```
To clean the GSC build use:
```bash
make clean
```
## Usage
```bash
Usage: gsc [option] [arguments] 
Available options: 
        compress - compress VCF/BCF file
        decompress     - query and decompress to VCF/BCF file
```
- Compress the input VCF/BCF file
```bash
Usage of gsc compress:

        gsc compress [options] [--in [in_file]] [--out [out_file]]

Where:

        [options]              Optional flags and parameters for compression.
        -i,  --in [in_file]    Specify the input file (default: VCF or VCF.GZ). If omitted, input is taken from standard input (stdin).
        -o,  --out [out_file]  Specify the output file. If omitted, output is sent to standard output (stdout).

Options:

        -M,  --mode_lossly     Choose lossy compression mode (lossless by default).
        -b,  --bcf             Input is a BCF file (default: VCF or VCF.GZ).
        -p,  --ploidy [X]      Set ploidy of samples in input VCF to [X] (default: 2).
        -t,  --threads [X]     Set number of threads to [X] (default: 1).
        -d,  --depth [X]       Set maximum replication depth to [X] (default: 100, 0 means no matches).
        -m,  --merge [X]       Specify files to merge, separated by commas (e.g., -m chr1.vcf,chr2.vcf), or '@' followed by a file containing a list of VCF files (e.g., -m @file_with_IDs.txt). By default, all VCF files are compressed.
```
- Decompress / Query
```bash
Usage of gsc decompress and query:

        gsc decompress [options] --in [in_file] --out [out_file]

Where:
        [options]              Optional flags and parameters for compression.
        -i,  --in [in_file]    Specify the input file . If omitted, input is taken from standard input (stdin).
        -o,  --out [out_file]  Specify the output file (default: VCF). If omitted, output is sent to standard output (stdout).

Options:

    General Options:

        -M,  --mode_lossly      Choose lossy compression mode (default: lossless).
        -b,  --bcf              Output a BCF file (default: VCF).

    Filter options (applicable in lossy compression mode only): 

        -r,  --range [X]        Specify range in format [chr]:[start],[end] (e.g., -r chr1:4999756,4999852).
        -s,  --samples [X]      Samples separated by comms (e.g., -s HG03861,NA18639) OR '@' sign followed by the name of a file with sample name(s) separated by whitespaces (for exaple: -s @file_with_IDs.txt). By default all samples/individuals are decompressed. 
        --header-only           Output only the header of the VCF/BCF.
        --no-header             Output without the VCF/BCF header (only genotypes).
        -G,  --no-genotype      Don't output sample genotypes (only #CHROM, POS, ID, REF, ALT, QUAL, FILTER, and INFO columns).
        -C,  --out-ac-an        Write AC/AN to the INFO field.
        -S,  --split            Split output into multiple files (one per chromosome).
        -I, [ID=^]              Include only sites with specified ID (e.g., -I "ID=rs6040355").
        --minAC [X]             Include only sites with AC <= X.
        --maxAC [X]             Include only sites with AC >= X.
        --minAF [X]             Include only sites with AF >= X (X: 0 to 1).
        --maxAF [X]             Include only sites with AF <= X (X: 0 to 1).
        --min-qual [X]          Include only sites with QUAL >= X.
        --max-qual [X]          Include only sites with QUAL <= X.
```
## Example
There is an example VCF/VCF.gz/BCF file, `toy.vcf`/`toy.vcf.gz`/`toy.bcf`, in the toy folder, which can be used to test GSC
### Compress

#### Lossless compression:
The input file format is VCF. You can compress a VCF file in lossless mode using one of the following methods:
1. **Explicit input and output file parameters**:
   
   Use the `--in` option to specify the input VCF file and the `--out` option for the output compressed file.
   ```bash
   ./bin/gsc compress --in toy/toy.vcf --out toy/toy_lossless.gsc
   ```
2. **Input file parameter and output redirection**:
   
   Use the `--out` option for the output compressed file and redirect the input VCF file into the command.
   ```bash
   ./bin/gsc compress --out toy/toy_lossless.gsc < toy/toy.vcf
   ```
3. **Output file redirection and input file parameter**:
   
   Specify the input VCF file with the `--in` option and redirect the output to create the compressed file.
   ```bash
   ./bin/gsc compress --in toy/toy.vcf > toy/toy_lossless.gsc
   ```
4. **Input and output redirection**:
   
   Use shell redirection for both input and output. This method does not use the `--in` and `--out` options.
   ```bash
   ./bin/gsc compress < toy/toy.vcf > toy/toy_lossless.gsc
   ```
This will create a file:
* `toy_lossless.gsc` - The compressed archive of the entire VCF file.

#### Lossy compression:

The input file format is VCF. The commands are similar to those used for lossless compression, with the addition of the `-M` parameter to enable lossy compression.

   For example, to compress a VCF file in lossy mode:

   ```bash
   ./bin/gsc compress -M --in toy/toy.vcf --out toy/toy_lossy.gsc
   ```
   or 
  
   Using redirection:
   ```bash
   ./bin/gsc compress -M --out toy/toy_lossy.gsc < toy/toy.vcf
   ``` 
   This will create a file:
   * `toy_lossy.gsc` - The compressed archive of the entire VCF file is implemented with lossy compression. It only retains the 'GT' subfield within the INFO and FORMAT fields, and excludes all other subfields..
    
### Decompress   (The commands are similar to those used for compression)
#### Lossless decompression:

To decompress the compressed toy_lossless.gsc into a VCF file named toy_lossless.vcf:
```bash
./bin/gsc decompress --in toy/toy_lossless.gsc --out toy/toy_lossless.vcf
```
#### Lossy decompression:

To decompress the compressed toy_lossy.gsc into a VCF file named toy_lossy.vcf:
```bash
./bin/gsc decompress -M --in toy/toy_lossy.gsc --out toy/toy_lossy.vcf
```
### Query
#### Variant-based query
Retrieve entries for chromosome 20 with POS ranging from 1 to 1,000,000, and output to the toy/query_toy_r_20_3_1000000.vcf file.

```bash
./bin/gsc decompress -M --range 20:1,1000000 --in toy/toy_lossy.gsc --out toy/query_toy_20_3_1000000.vcf
```
Retrieve entries for chromosome 20 with POS ranging from 1 to 1,000,000, and output to the terminal interface.
```bash
./bin/gsc decompress -M --range 20:1,1000000 --in toy/toy_lossy.gsc
```
#### Sample-based query
Retrieve genotype columns for samples named NA00001 and NA00002, and output to the toy/query_toy_s_NA00001_NA00002.vcf file.
```bash
./bin/gsc decompress -M --samples NA00001,NA00002 --in toy/toy_lossy.gsc --out toy/query_toy_s_NA00001_NA00002.vcf
```
or

The names NA00001 and NA00002 are stored in the toy/samples_name_file.
```bash
./bin/gsc decompress -M --samples @toy/samples_name_file --in toy/toy_lossy.gsc --out toy/query_toy_s_NA00001_NA00002.vcf
```
#### Note
You can also perform mixed queries based on sample names and variants.

## Citations
- **bio.tools ID**: `gsc_genotype_sparse_compression`
- **Research Resource Identifier (RRID)**: `SCR_025071`
- **Doi**:`https://doi.org/10.48546/WORKFLOWHUB.WORKFLOW.887.1`
