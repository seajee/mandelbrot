
mandelbrot: main.c
	cc -Wall -Wextra -O3 -o mandelbrot main.c -lraylib -lm

clean:
	rm -rf mandelbrot
