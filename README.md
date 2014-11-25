Evolve image compessors with genetic programming
================================================

This is the repo for my undergraduate thesis.

The current idea (will probably change significantly):

1. Use an evolutionary algorithm to find sets of functions that can be used in linear combinations to express a set of data points (aka. a discrete transform)
2. Modify a home-brewed GPU JPEG encoder, replacing the discrete cosine transform by the fittest set of functions obtained from step 1.
3. Use different metrics: Compression ratio, image delta, and excecution speed, to compare the evolved transform to the standard JPEG DCT.

To Do
=====

* GPU JPEG encoder.
* Benchmark GPU-JPEG against a good OSS encoder.
* GP algorithm that finds discrete transforms & integration with encoder.
