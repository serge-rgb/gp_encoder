clang jpeg_test.c -Weverything -fsanitize=address -I tiny_jpeg/ -pthread -lm && ./a.out
#gcc jpeg_test.c -Wall -Wpedantic -std=c99 -I tiny_jpeg/ -pthread -lm && ./a.out
