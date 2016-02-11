OpenDcd - An Open Source WFST based Speech Recognition Decoder
=================

OpenDcd is a lightweight and portable WFST based speech decoding toolkit written in C++. OpenDcd provides a set of tools for decoding, cascade construction and hypothesis post-processing. The focus of the toolkit is to provide a foundation for research into new decoding methods that can be deployed. Through the use of C++ templates the core decoder can be configured and extended in many ways. For example selecting different on-the-fly composition or lattice generation strategies. The core engine has detailed profiling, logging and analysis methods that make it highly for deployement in production systems. The toolkit makes used of [OpenFst](http://openfst.org/) for representing and manipulating the models. It is distributed as an open source project with an Apache Licence.

For more information see the [main documentation site](https://github.com/opendcd/opendcd.github.io/wiki), and the tutorial for installing the OpenDcd and decoding using the Librispeech corpus and models from kaldi-asr.og.


Quick Installation Guide
-------------------------

````bash
    export KALDI_ROOT=/path/to/kaldi-trunk/
    git clone https://github.com/opendcd/opendcd.git
    cd opendcd/3rdparty
    make
    cd ../src
    make
````

Kaldi Conversion Quick Start
-------------------------------

For Kaldi model conversion and decoding a working Kaldi installation and 
set of acoustic and language models and features from generated from a Kaldi egs/s5 
script are required. The following example is based on the output of Kaldi WSJ training run.

Graph construction, the scripts directory contains 
The Kaldi language directory, we reuse the existing Kaldi lexicon and LM.

````bash
    cd $OPENDCD/scripts
    export KALDI_ROOT=/home/opendcd/tools/kaldi-trunk
    ./makeclevel.sh \
    $KALDI_ROOT/egs/wsj/s5/data/lang_test_bg_5k \
    $KALDI_ROOT/egs/wsj/s5/exp/tri2a \
    $KALDI_ROOT/egs/wsj/s5/exp/ocd_tri2a \
    $KALDI_ROOT
````


See egs directory contains example script for showing how to convert a Kaldi WSJ setup

Brief Overview
---------------

The first release includes the following features:

  - Standalone lightweight decoder core
  - Kaldi file format compatible or optionally build against Kaldi
  - Post-processing tools
  - OpenFst and Kaldi Interop Tools

Decoder:

  - Customizable transition model for custom user and transition models
  - Direct decoding on different weight semiring
  - On-the-fly decoding using lookahead composition
  - Lattice generation
  - Switchable STL implementations. Use different implementations such EASTL or RDESTL, or mix optimized containers such as Google sparse hash.
  - Powerful registration mechanism for adding user defined acoustic models and or lattice generation strategies

Cascade construction:

  - Script to efficiently build and convert models from a Kaldi lang directory
  
Results post-processing:

  - ``farfilter`` Apply the command to every in FST in the FAR archive
  - ``latticetofar`` Convert Kaldi Table to OpenFst FAR archive
  - ``fartolattice`` Convert an OpenFst FAR archive to Kaldi Table

Kaldi Interoperability:

  - Write results in Kaldi *Lattice* table format
  - [More information](https://github.com/opendcd/opendcd.github.io/wiki/Kaldi-Interoperability) on optionally building against Kaldi 
  - Convert Kaldi tree to optimized decoding structure

More Information
  - A [getting start guide](https://github.com/opendcd/opendcd.github.io/wiki/EC2-Installation-Walkthrough) for running OpenDcd on Ec2 using the Librispeech models
  - Ongoing introductory slides can be found [here](https://dl.dropboxusercontent.com/u/321851/opendcd.pdf). These are updated infrequently. 

We request acknowledgement in any publications that make use of this software
cites the below paper.

```latex
@Article{dixon:2015dcd,
     author    = "Paul R. Dixon",
     title     = "{OpenDcd: An Open Source WFST Decoding Toolkit}",
     year      = "2014"
}
```

