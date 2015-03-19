Evolved JPEG compessors with genetic programming
================================================

This is an image encoder that outputs JPEG files.

At its core, it is a baseline JPEG lossy compression encoder. Wrapping the encoder is an genetic algorithm that evolves quantization matrices towards the desired quality:compression ratio.

This means that the encoder actually compresses the file many times, calculating the compression error and selecting the best encoder through many generations.

Rationale: JPEGs are decoded many more times than they are encoded. It makes sense to use our compute resources to generate an optimal JPEG.

This library is useful for applications that use JPEG and that really care about size. For example, a photo sharing service with millions of users might want to spend a little more time encoding to save a lot of space without much loss in perceptible quality.

Usage
-----
Right now, this is a C99 header-only API. It works, but there is no obvious way to specify the desired quality of the final image.


To Do
=====

* GPU JPEG encoder. -- Use the GPU to encode.
* Polish and create command-line tool.
* Support pthreads (make it work on OSX/Linux)
