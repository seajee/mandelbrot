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

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 400
#define INITIAL_SCALE 2.0
#define INITIAL_RESOLUTION 0.5
#define INITIAL_ITERATIONS 100
#define MANDEL_INFINITY 16.0
#define SPEED 0.5

#define OUTPUT_WIDTH 4000 // 16384
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

void render(Vector2Double camera, Vector2Double scale, double resolution, int iterations, bool realtime)
{
    int width;
    int height;
    int pixel_width;
    int pixel_height;

    struct timespec start, end;
    int comp = 3;
    uint8_t *pixels = NULL;
    int prev_percent = -1;

    if (realtime) {
        width = GetScreenWidth();
        height = GetScreenHeight();
    } else {
        printf("INFO: Starting rendering\n");
        clock_gettime(CLOCK_MONOTONIC, &start);

        double screen_ratio = (double)GetScreenHeight() / GetScreenWidth();
        width = OUTPUT_WIDTH;
        height = width * screen_ratio;

        pixels = malloc(width * height * comp * sizeof(*pixels));
        assert(pixels != NULL);
    }

    pixel_width = width / (width * resolution);
    pixel_height = height / (height * resolution);

    for (int y = 0; y < height; y += pixel_height) {
        if (!realtime) {
            int percent = (double)y / height * 100.0;
            if (percent != prev_percent) {
                printf("%d%%\r", percent);
                fflush(stdout);
                prev_percent = percent;
            }
        }

        for (int x = 0; x < width; x += pixel_width) {
            double z_real = map(x, 0, width,  camera.x - scale.x, camera.x + scale.x);
            double z_imag = map(y, 0, height, camera.y - scale.y, camera.y + scale.y);
            double c_real = z_real;
            double c_imag = z_imag;

            int i;
            for (i = 0; i < iterations; ++i) {
                double new_z_real = z_real*z_real - z_imag*z_imag;
                double new_z_imag = 2*z_real*z_imag;

                z_real = new_z_real + c_real;
                z_imag = new_z_imag + c_imag;

                if (fabs(z_real + z_imag) > MANDEL_INFINITY) {
                    break;
                }
            }

            Color color = BLACK;
            if (i != iterations) {
                double norm = normalize(i, 0.0, iterations);
                double bright = sqrt(norm) * 255;
                color = (Color){ bright, bright, bright, 255 };
            }

            if (realtime) {
                DrawRectangle(x, y, pixel_width, pixel_height, color);
            } else {
                int pix = (x + y*width) * comp;
                pixels[pix + 0] = color.r;
                pixels[pix + 1] = color.g;
                pixels[pix + 2] = color.b;
            }
        }
    }

    if (!realtime) {
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
}

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "mandelbrot");

    double screen_ratio = (double)WINDOW_HEIGHT / WINDOW_WIDTH;
    Vector2Double camera = { 0.0, 0.0 };
    Vector2Double scale = { INITIAL_SCALE, INITIAL_SCALE * screen_ratio };
    double resolution = INITIAL_RESOLUTION;
    int iterations = INITIAL_ITERATIONS;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int width = GetScreenWidth();
        int height = GetScreenHeight();
        screen_ratio = (double)height / width;
        scale.y = scale.x * screen_ratio;

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            scale.x -= SPEED * scale.x * dt;
            scale.y -= SPEED * scale.y * dt;
        }
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            scale.x += SPEED * scale.x * dt;
            scale.y += SPEED * scale.y * dt;
        }
        scale.x += -1.0 * GetMouseWheelMove() * SPEED * scale.x;
        scale.y += -1.0 * GetMouseWheelMove() * SPEED * scale.y;

        if (IsKeyDown(KEY_W)) {
            camera.y -= SPEED * scale.y * dt;
        }
        if (IsKeyDown(KEY_A)) {
            camera.x -= SPEED * scale.x * dt;
        }
        if (IsKeyDown(KEY_S)) {
            camera.y += SPEED * scale.y * dt;
        }
        if (IsKeyDown(KEY_D)) {
            camera.x += SPEED * scale.x * dt;
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
            render(camera, scale, 1.0, OUTPUT_ITERATIONS, false);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        render(camera, scale, resolution, iterations, true);

        DrawText(TextFormat("Iterations: %i", iterations), 10, 10, 20, GREEN);
        DrawText(TextFormat("Resolution: %f", resolution), 10, 30, 20, GREEN);
        DrawText(TextFormat("Scale: (%f, %f)", scale.x, scale.y), 10, 50, 20, GREEN);
        DrawText(TextFormat("Camera: (%f, %f)", camera.x, -camera.y), 10, 70, 20, GREEN);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
