#version 330

#define MANDEL_INFINITY 16.0

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

uniform vec2 u_Resolution;
uniform vec2 u_Camera;
uniform vec2 u_Scale;
uniform int u_Iterations;

float map(float value, float inputStart, float inputEnd, float outputStart, float outputEnd)
{
    float result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
    return result;
}

vec2 square_complex(vec2 c)
{
    return vec2(
        c.x * c.x - c.y * c.y,
        2 * c.x * c.y
    );
}

int inside_mandelbrot_set(vec2 point)
{
    vec2 z = vec2(
        map(point.x, 0, u_Resolution.x, u_Camera.x - u_Scale.x, u_Camera.x + u_Scale.x),
        map(point.y, 0, u_Resolution.y, u_Camera.y - u_Scale.y, u_Camera.y + u_Scale.y)
    );
    vec2 c = z;

    int i;
    for (i = 0; i < u_Iterations; ++i) {
        z = square_complex(z) + c;

        if (abs(z.x + z.y) > MANDEL_INFINITY) {
            return i;
        }
    }

    return i;
}

void main()
{
    vec2 pixel = gl_FragCoord.xy;
    pixel.y = u_Resolution.y - pixel.y; // Flip y coord

    int iters = inside_mandelbrot_set(pixel);
    if (iters == u_Iterations) {
        finalColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float norm = float(iters) / float(u_Iterations);
        float bright = sqrt(norm);
        finalColor = vec4(bright, bright, bright, 1.0);
    }
}
