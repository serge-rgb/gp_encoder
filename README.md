Evolve image compessors with genetic programming
================================================

This is the repo for my undergraduate thesis.

The current idea (iteration #2):

Create an encoder that runs JPEG many times, evolving in the process to end up with better compression.

For a specific image, use an evolutionary algorithm to mutate quantization matrices; evolve a close-to-ideal matrix, using the error function as the fitness.


To Do
=====

* JPEG encoder.
* Design a genetic algorithm
* GPU JPEG encoder.
