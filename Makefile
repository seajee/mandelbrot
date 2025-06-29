
mandelbrot: main.c
	cc -Wall -Wextra -O3 -fopenmp -o mandelbrot main.c -lraylib -lm

clean:
	rm -rf mandelbrot
