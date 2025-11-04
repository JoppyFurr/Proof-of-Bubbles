/*
 * Calculations for bubble angles.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <math.h>

int main (void)
{
    /* The minimum angle rises 1 px vertical per 10px horizontal. */
    double minimum_angle = atan (1.0 / 10.0);
    printf ("Minimum angle: %2.2f°\n", minimum_angle * 180.0 / M_PI);

    /* The full sweep is 180° minus the minimum angle at either end. */
    double full_sweep = M_PI - 2.0 * minimum_angle;

    /* There are 121 possible angles to choose, so 120 intervals */
    double step_size = full_sweep / 120.0;

    for (int32_t step = -60; step <= 60; step++)
    {
        double angle = minimum_angle + (step + 60) * step_size;

        /* The bubble velocity is 5px per frame. Use 8.8 fixed-precision */
        double delta_x = -5.0 * cos (angle);
        double delta_y = -5.0 * sin (angle);
        int16_t delta_x_fixed = round (delta_x * 256.0);
        int16_t delta_y_fixed = round (delta_y * 256.0);
        printf ("step [%3d]: %6.2f° { x = %4.1f, y = %4.1f };  { .x = 0x%04x, .y = 0x%04x };\n",
                step, angle * 180.0 / M_PI, delta_x, delta_y,
                (uint16_t) delta_x_fixed, (uint16_t) delta_y_fixed);
    }

    return EXIT_SUCCESS;
}
