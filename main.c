#include <assert.h>
#include <omp.h>
#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WINDOW_SIZE 1000
#define INITIAL_RESOLUTION 0.5
#define INITIAL_ITERATIONS 100
#define MANDEL_INFINITY 16.0
#define SPEED 0.5

#define OUTPUT_RESOLUTION 4000 // 16384
#define OUTPUT_ITERATIONS 4000
#define OUTPUT_PATH "output.png"

typedef struct {
    double x;
    double y;
} Vector2Double;

double map(double value, double inputStart, double inputEnd, double outputStart, double outputEnd)
{
    double result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;

    return result;
}

double normalize(double value, double start, double end)
{
    double result = (value - start)/(end - start);

    return result;
}

double clamp(double value, double min, double max)
{
    double result = (value < min)? min : value;

    if (result > max) result = max;

    return result;
}

void render_to_image(Vector2Double camera, double scale)
{
    printf("INFO: Starting rendering\n");

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int width = OUTPUT_RESOLUTION;
    int height = OUTPUT_RESOLUTION;
    int comp = 3;
    int size = width * height * comp;

    uint8_t *pixels = malloc(size * sizeof(*pixels));
    assert(pixels != NULL);

    int prev_percent = -1;

    for (int y = 0; y < height; ++y) {
        int percent = (double)y / height * 100.0;
        if (percent != prev_percent) {
            printf("%d%%\r", percent);
            fflush(stdout);
            prev_percent = percent;
        }

#pragma omp parallel for
        for (int x = 0; x < width; ++x) {
            double a = map(x, 0, width,  camera.x - scale, camera.x + scale);
            double b = map(y, 0, height, camera.y - scale, camera.y + scale);
            double ca = a;
            double cb = b;

            int i;
            for (i = 0; i < OUTPUT_ITERATIONS; ++i) {
                double aa = a*a - b*b;
                double bb = 2*a*b;

                a = aa + ca;
                b = bb + cb;

                if (fabs(a + b) > MANDEL_INFINITY) {
                    break;
                }
            }

            int bright = 0;
            // Color color = BLACK;
            if (i != OUTPUT_ITERATIONS) {
                double norm = normalize(i, 0.0, OUTPUT_ITERATIONS);
                // double n = (a + b) / MANDEL_INFINITY;
                bright = sqrtf(norm) * 255;
                // color = ColorFromHSV(sqrtf(n) * 360.0f, 1.0f, 1.0f);
            }

            int pix = (x + y*width) * comp;
            pixels[pix + 0] = bright;
            pixels[pix + 1] = bright;
            pixels[pix + 2] = bright;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    long delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec -
            start.tv_nsec) / 1000;
    long ms = delta_us / 1000;

    printf("INFO: Rendering took %ldms\n", ms);
    printf("INFO: Starting saving\n");

    clock_gettime(CLOCK_MONOTONIC, &start);

    int res = stbi_write_png(OUTPUT_PATH, width, height, comp, pixels, width * comp);
    if (res == 0) {
        fprintf(stderr, "ERROR: Could not render output image\n");
    } else {
        clock_gettime(CLOCK_MONOTONIC, &end);
        delta_us = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_nsec -
                start.tv_nsec) / 1000;
        ms = delta_us / 1000;
        printf("INFO: Saving took %ldms\n", ms);
    }

    free(pixels);
}

int main(void)
{
    InitWindow(WINDOW_SIZE, WINDOW_SIZE, "mandelbrot");

    Vector2Double camera = { 0.0, 0.0 };
    double scale = 2.0;
    double resolution = INITIAL_RESOLUTION;
    int iterations = INITIAL_ITERATIONS;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int width = GetScreenWidth();
        int height = GetScreenHeight();
        int pixel_width = width / (width * resolution);
        int pixel_height = height / (height * resolution);

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            scale -= SPEED * scale * dt;
        }
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            scale += SPEED * scale * dt;
        }
        scale += -1.0 * GetMouseWheelMove() * SPEED * scale;

        if (IsKeyDown(KEY_W)) {
            camera.y -= SPEED * scale * dt;
        }
        if (IsKeyDown(KEY_A)) {
            camera.x -= SPEED * scale * dt;
        }
        if (IsKeyDown(KEY_S)) {
            camera.y += SPEED * scale * dt;
        }
        if (IsKeyDown(KEY_D)) {
            camera.x += SPEED * scale * dt;
        }

        if (IsKeyPressed(KEY_RIGHT_SHIFT)) {
            resolution *= 2.0;
            resolution = clamp(resolution, 0.0, 1.0);
        }
        if (IsKeyPressed(KEY_RIGHT_CONTROL)) {
            resolution /= 2.0;
        }

        if (IsKeyPressed(KEY_LEFT_SHIFT)) {
            iterations += 100;
        }
        if (IsKeyPressed(KEY_LEFT_CONTROL)) {
            iterations -= 100;
            if (iterations < 0) iterations = 0;
        }

        if (IsKeyPressed(KEY_R)) {
            render_to_image(camera, scale);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        for (int y = 0; y < height; y += pixel_height) {
            for (int x = 0; x < width; x += pixel_width) {
                double a = map(x, 0.0, width,  camera.x - scale, camera.x + scale);
                double b = map(y, 0.0, height, camera.y - scale, camera.y + scale);
                double ca = a;
                double cb = b;

                int i;
                for (i = 0; i < iterations; ++i) {
                    double aa = a*a - b*b;
                    double bb = 2*a*b;

                    a = aa + ca;
                    b = bb + cb;

                    if (fabs(a + b) > MANDEL_INFINITY) {
                        break;
                    }
                }

                Color color = BLACK;
                if (i != iterations) {
                    double norm = normalize(i, 0.0, iterations);
                    double bright = sqrt(norm) * 255;
                    color = (Color){ bright, bright, bright, 255 };
                }

                DrawRectangle(x, y, pixel_width, pixel_height, color);
            }
        }

        DrawText(TextFormat("Iterations: %i", iterations), 10, 10, 20, GREEN);
        DrawText(TextFormat("Resolution: %f", resolution), 10, 30, 20, GREEN);
        DrawText(TextFormat("Scale: %f", scale), 10, 50, 20, GREEN);
        DrawText(TextFormat("Camera: (%f, %f)", camera.x, -camera.y), 10, 70, 20, GREEN);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
