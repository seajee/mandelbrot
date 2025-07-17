#include <assert.h>
#include <pthread.h>
#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define FONT_SIZE 20
#define INITIAL_SCALE 2.0
#define INITIAL_RESOLUTION 0.25
#define INITIAL_ITERATIONS 100
#define MANDEL_INFINITY 16.0
#define SPEED 0.5

#define OUTPUT_WIDTH 4000 // 16384
#define OUTPUT_ITERATIONS 4000
#define OUTPUT_PATH "output.png"

// TODO: Shaders need this to be a float and not double
typedef float real;

typedef struct {
    real x;
    real y;
} Vector2Real;

typedef struct {
    Vector2Real camera;
    Vector2Real scale;
} RenderArgs;

real map(real value, real inputStart, real inputEnd, real outputStart, real outputEnd);
real normalize(real value, real start, real end);
real clamp(real value, real min, real max);
void render_frame(Vector2Real camera, Vector2Real scale, real resolution, int iterations);
void render_image(Vector2Real camera, Vector2Real scale);
void *render_thread(void *arg);
void render(Vector2Real camera, Vector2Real scale, real resolution, int iterations, bool realtime);

// Globals
static bool g_rendering_image = false;
static int g_rendering_percent = 0;

int main(void)
{
    // Initialize window
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "mandelbrot");
    SetTargetFPS(60);

    // Load shaders and their uniforms
    Shader shader = LoadShader("base.vert", "mandelbrot.frag");
    int u_resolution = GetShaderLocation(shader, "u_Resolution");
    int u_camera = GetShaderLocation(shader, "u_Camera");
    int u_scale = GetShaderLocation(shader, "u_Scale");
    int u_iterations = GetShaderLocation(shader, "u_Iterations");

    // Screen resolution
    real screen_ratio = (real)WINDOW_HEIGHT / WINDOW_WIDTH;
    Vector2Real screen_size = { WINDOW_WIDTH, WINDOW_HEIGHT };

    // Rendering variables
    Vector2Real camera = { -0.5, 0.0 };
    Vector2Real scale = { INITIAL_SCALE, INITIAL_SCALE * screen_ratio };
    real resolution = INITIAL_RESOLUTION;
    int iterations = INITIAL_ITERATIONS;

    // Toggles
    bool debug = true;
    bool gpu = false;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        int width = GetScreenWidth();
        int height = GetScreenHeight();
        screen_size.x = width;
        screen_size.y = height;
        screen_ratio = (real)height / width;
        scale.y = scale.x * screen_ratio;

        /* Input handling */

        // Scale
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

        // Position
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

        // Resolution
        if (IsKeyPressed(KEY_RIGHT_SHIFT)) {
            resolution *= 2.0;
            resolution = clamp(resolution, 0.0, 1.0);
        }
        if (IsKeyPressed(KEY_RIGHT_CONTROL)) {
            resolution /= 2.0;
        }

        // Iterations
        if (IsKeyPressed(KEY_LEFT_SHIFT)) {
            iterations += 100;
        }
        if (IsKeyPressed(KEY_LEFT_CONTROL)) {
            iterations -= 100;
            if (iterations < 0) iterations = 0;
        }

        // Image rendering
        if (IsKeyPressed(KEY_R) && !g_rendering_image) {
            render_image(camera, scale);
        }

        // Toggles
        if (IsKeyPressed(KEY_B)) {
            debug = !debug;
        }
        if (IsKeyPressed(KEY_G)) {
            gpu = !gpu;
        }

        /* Rendering */

        BeginDrawing();
        ClearBackground(BLACK);

        // Draw Mandelbrot set
        if (gpu) {
            BeginShaderMode(shader);
            SetShaderValue(shader, u_resolution, &screen_size, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, u_camera, &camera, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, u_scale, &scale, SHADER_UNIFORM_VEC2);
            SetShaderValue(shader, u_iterations, &iterations, SHADER_UNIFORM_INT);
            DrawRectangle(0, 0, width, height, WHITE);
            EndShaderMode();
        } else {
            render_frame(camera, scale, resolution, iterations);
        }

        // Debug info text
        if (debug) {
            int i = 0;
            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
            DrawText(TextFormat("Iterations: %i", iterations), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
            DrawText(TextFormat("Resolution: %f", (gpu ? 1.0 : resolution)), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
            DrawText(TextFormat("Scale: (%f, %f)", scale.x, scale.y), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
            DrawText(TextFormat("Camera: (%f, %f)", camera.x, -camera.y), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
            DrawText(TextFormat("Rendering mode: %s", (gpu ? "GPU" : "CPU")), 10, 10 + 20*(i++), FONT_SIZE, GREEN);
        }
        if (g_rendering_image) {
            const char *text = "Rendering "OUTPUT_PATH" (Saving)";
            if (g_rendering_percent != -1) {
                text = TextFormat("Rendering "OUTPUT_PATH" (%d%%)", g_rendering_percent);
            }
            int text_width = MeasureText(text, FONT_SIZE);
            int x = width/2 - text_width/2;
            DrawText(text, x, 10, 20, RED);
        }

        EndDrawing();
    }

    // Cleanup
    UnloadShader(shader);
    CloseWindow();

    return EXIT_SUCCESS;
}

real map(real value, real inputStart, real inputEnd, real outputStart, real outputEnd)
{
    real result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;

    return result;
}

real normalize(real value, real start, real end)
{
    real result = (value - start)/(end - start);

    return result;
}

real clamp(real value, real min, real max)
{
    real result = (value < min)? min : value;

    if (result > max) result = max;

    return result;
}

void render_frame(Vector2Real camera, Vector2Real scale, real resolution, int iterations)
{
    render(camera, scale, resolution, iterations, true);
}

void render_image(Vector2Real camera, Vector2Real scale)
{
    RenderArgs *args = malloc(sizeof(*args));
    assert(args != NULL);
    args->camera = camera;
    args->scale = scale;

    pthread_t tid;
    if (pthread_create(&tid, NULL, render_thread, args) != 0) {
        fprintf(stderr, "ERROR: Could not create the image rendering thread\n");
    }
}

void *render_thread(void *arg)
{
    RenderArgs *args = (RenderArgs*)arg;
    g_rendering_image = true;
    render(args->camera, args->scale, 1.0, OUTPUT_ITERATIONS, false);
    g_rendering_image = false;
    free(args);
    return NULL;
}

void render(Vector2Real camera, Vector2Real scale, real resolution, int iterations, bool realtime)
{
    int width;
    int height;
    int pixel_width;
    int pixel_height;

    struct timespec start, end;
    int comp = 3;
    uint8_t *pixels = NULL;

    if (realtime) {
        width = GetScreenWidth();
        height = GetScreenHeight();
    } else {
        clock_gettime(CLOCK_MONOTONIC, &start);

        real screen_ratio = (real)GetScreenHeight() / GetScreenWidth();
        width = OUTPUT_WIDTH;
        height = width * screen_ratio;

        pixels = malloc(width * height * comp * sizeof(*pixels));
        assert(pixels != NULL);
    }

    pixel_width = width / (width * resolution);
    pixel_height = height / (height * resolution);

    for (int y = 0; y < height; y += pixel_height) {
        if (!realtime) {
            g_rendering_percent = (real)y / height * 100.0;
        }

        for (int x = 0; x < width; x += pixel_width) {
            real z_real = map(x, 0, width,  camera.x - scale.x, camera.x + scale.x);
            real z_imag = map(y, 0, height, camera.y - scale.y, camera.y + scale.y);
            real c_real = z_real;
            real c_imag = z_imag;

            int i;
            for (i = 0; i < iterations; ++i) {
                real new_z_real = z_real*z_real - z_imag*z_imag;
                real new_z_imag = 2*z_real*z_imag;

                z_real = new_z_real + c_real;
                z_imag = new_z_imag + c_imag;

                if (fabs(z_real + z_imag) > MANDEL_INFINITY) {
                    break;
                }
            }

            Color color = BLACK;
            if (i != iterations) {
                real norm = normalize(i, 0.0, iterations);
                real bright = sqrt(norm) * 255;
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

        clock_gettime(CLOCK_MONOTONIC, &start);

        g_rendering_percent = -1;
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
