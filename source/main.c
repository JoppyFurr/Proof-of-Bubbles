/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SMSlib.h"

#include "angles.h"

#define TARGET_SMS
#include "../game_tile_data/patterns.h"
#include "../game_tile_data/pattern_index.h"
#include "../game_tile_data/palette.h"

/* Global state */
uint8_t cursor_x = 128;
uint8_t cursor_y = 96;

#define LAUNCHER_AIM_MIN      0
#define LAUNCHER_AIM_CENTRE  60
#define LAUNCHER_AIM_MAX    120

/* Coordinates in 8.8 fixed-point format */
#define LEFT_EDGE        0x4000
#define RIGHT_EDGE       0xb000
#define LAUNCH_FROM_X    0x7800
#define LAUNCH_FROM_Y    0x9a00

uint8_t launcher_aim = LAUNCHER_AIM_CENTRE;
uint16_t active_bubble_velocity_x = 0;
uint16_t active_bubble_velocity_y = 0;
uint16_t active_bubble_x = LAUNCH_FROM_X;
uint16_t active_bubble_y = LAUNCH_FROM_Y;

typedef enum bubble_e {
    BUBBLE_NONE = 0,
    BUBBLE_CYAN,
    BUBBLE_RED,
    BUBBLE_GREEN,
    BUBBLE_YELLOW,
    BUBBLE_MAX
} bubble_t;

typedef enum game_state_e {
    BUBBLE_READY = 0,
    BUBBLE_MOVING
} game_state_t;

#define BOARD_ROWS 11
#define BOARD_COLS 8

/* Using a linear game-board, neighbouring bubbles can be checked with
 * simple offsets. An border of unset bubbles is included in the array
 * to avoid the need for bounds-checking.
 *
 *  -> 6 rows have 8 bubbles (8 rows of 10 bubbles considering the border)
 *  -> 5 rows have 7 bubbles (9 rows of 9 bubbles considering the border)
 *
 *  8 * 10 + 9 * 9 = 161 positions in the array.
 */
bubble_t game_board [161];
#define NEIGH_TOP_LEFT    -10
#define NEIGH_TOP_RIGHT    -9
#define NEIGH_LEFT         -1
#define NEIGH_right         1
#define NEIGH_BOTTOM_LEFT   9
#define NEIGH_BOTTOM_RIGHT 10

/* Using a simple division by 14 to get the row, and division by 16 (after accounting
 * for stagger) to get the column gets a close estimate of a pixel's game-board position.
 * However, the bubbles aren't rectangular,  so the pixel coordinate within the rectangle
 * index the pixel-to-board array to reach the correct the game-board position */
static const int8_t pixel_to_board [14] [16] = {
    { -10, -10, -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  -9,  -9,  -9 },
    { -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  -9 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0 },
    {  +9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, +10 },
    {  +9,  +9,  +9,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, +10, +10, +10 }
};

static const uint32_t blue_tile [32] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                         0x00000000, 0x00000000, 0x00000000, 0x00000000 };

static const uint32_t black_tile [32] = { 0xff0000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                          0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };

/*
 * A debug cursor to set and unset bubbles in the game board.
 */
void draw_cursor (void)
{
    SMS_addSprite (cursor_x,     cursor_y,     (uint8_t) (322    ));
    SMS_addSprite (cursor_x + 8, cursor_y,     (uint8_t) (322 + 1));
    SMS_addSprite (cursor_x,     cursor_y + 8, (uint8_t) (322 + 2));
    SMS_addSprite (cursor_x + 8, cursor_y + 8, (uint8_t) (322 + 3));
}


/*
 * Draw the active bubble, which is either sitting in the launcher,
 * or moving through the game board.
 */
void draw_active_bubble (void)
{
    /* TODO: Union or pointer math to avoid the shifts */
    uint8_t x = active_bubble_x >> 8;
    uint8_t y = active_bubble_y >> 8;
    SMS_addSprite (x,     y,     (uint8_t) (326    ));
    SMS_addSprite (x + 8, y,     (uint8_t) (326 + 1));
    SMS_addSprite (x,     y + 8, (uint8_t) (326 + 2));
    SMS_addSprite (x + 8, y + 8, (uint8_t) (326 + 3));
}


/*
 * An initial indicator of aiming direction.
 * TODO: Replace with an arrow.
 */
void draw_pip (void)
{
    uint8_t pip_x = 127 + (angle_data [launcher_aim].x >> 6);
    uint8_t pip_y = 161 + (angle_data [launcher_aim].y >> 6);
    SMS_addSprite (pip_x, pip_y, (uint8_t) (334));
}


/*
 * Draw a bubble to the screen at the specified game-board location.
 */
#define BYTES_PER_STRIP 640
#define BYTES_PER_ROW 56
void set_bubble (uint8_t position, bubble_t bubble)
{
    game_board [position] = bubble;

    /* Neighbours */
    bubble_t neigh_tl = game_board [position + NEIGH_TOP_LEFT];
    bubble_t neigh_tr = game_board [position + NEIGH_TOP_RIGHT];
    bubble_t neigh_bl = game_board [position + NEIGH_BOTTOM_LEFT];
    bubble_t neigh_br = game_board [position + NEIGH_BOTTOM_RIGHT];

    /* VRAM coordinates */
    uint16_t left_strip;
    uint16_t right_strip;

    uint8_t col = (position - 10) % 19;
    uint8_t row = (position - 10) / 19 * 2;

    /* Odd rows are offset by one strip */
    if (col > 8)
    {
        col -= NEIGH_BOTTOM_RIGHT;
        row += 1;
        left_strip = ((col << 1) + 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    else
    {
        left_strip = (col << 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    right_strip = left_strip + BYTES_PER_STRIP;

    /* Bubble panels are laid out in a square grid.
     *  - Each row contains the bubble to draw {none, cyan, red, green}
     *  - Each column contains the specific neighbour {none, cyan, red, green}
     */

    /* TODO - Can the below be improved a bit?
     * Consider:
     *  - Avoid the left-shift by having Sneptile provide uint32_t indices instead of pattern indices.
     */

    /* Left strip */
    SMS_VRAMmemcpy (left_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tl] [0] << 3], 32);
    SMS_VRAMmemcpy (left_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_bl] [2] << 3], 32);

    /* Right strip */
    SMS_VRAMmemcpy (right_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tr] [1] << 3], 32);
    SMS_VRAMmemcpy (right_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_br] [3] << 3], 32);
}


/*
 * Debug cursor handling,
 * controlled by player-2 inputs.
 */
void debug_cursor (uint16_t key_pressed, uint16_t key_status)
{
    /* Slow movement while holding button-2 */
    if (key_status & PORT_B_KEY_2)
    {
        key_status = key_pressed;
    }

    if ((key_status & PORT_B_KEY_UP) && cursor_y > 9)
    {
        cursor_y -= 1;
    }
    else if ((key_status & PORT_B_KEY_DOWN) && cursor_y < 162)
    {
        cursor_y += 1;
    }

    if ((key_status & PORT_B_KEY_LEFT) && cursor_x > 64)
    {
        cursor_x -= 1;
    }
    else if ((key_status & PORT_B_KEY_RIGHT) && cursor_x < 191)
    {
        cursor_x += 1;
    }

    /* Conversion of pixel coordinates into game-board position:
     *
     * There is a repeating pattern, every two rows of bubbles.
     * This pattern is made up of 28 pixel lines, and represents an increment
     * of +19 game board positions.
     *
     * the beginning of the game board (y = 9), then it has the following property:
     * Beginning one line into the game board area (y = 9), imagining that the game board is made of rectangles.
     * Staggered like bricks, rather than a hex-grid for circles:
     *
     *   - Dividing the Y coordinate by 14 will give the row.
     *   - Dividing the X position by 16 (offset by 8 every other row) will give the column
     *
     * Looking at the pixel position within the rectangles, an array of offsets can be used
     * to adjust the coordinate into the correct hex-grid game-board position:
     *
     *  - Two lines where the nearest bubble may be on the row above
     *  - Ten lines where the division by 14 was already correct
     *  - Two lines where the nearest bubble may be on the row below.
     */

    uint8_t repetition = (cursor_y - 9) / 28;
    uint8_t pos_y =      (cursor_y - 9) % 28;
    uint8_t pos_x =      (cursor_x - 64);

    uint8_t target_bubble = 10 + 19 * repetition;

    /* 8-bubble row */
    if (pos_y < 14)
    {
        target_bubble += pos_x >> 4;
        target_bubble += pixel_to_board [pos_y] [pos_x & 0x0f];

    }
    /* 7-bubble row */
    else
    {
        target_bubble += NEIGH_BOTTOM_LEFT;
        target_bubble += (pos_x + 8) >> 4;
        target_bubble += pixel_to_board [pos_y - 14] [(pos_x + 8) & 0x0f];
    }

    /* Don't draw to an invalid position */
    if (target_bubble < 10 || target_bubble > 112 ||
        (target_bubble - 10) % 19 == 9 ||
        (target_bubble - 10) % 19 == 17)
    {
        return;
    }

    if (key_pressed & PORT_B_KEY_1)
    {
        /* Cycle through bubble colours */
        static uint8_t next_bubble = BUBBLE_CYAN;
        set_bubble (target_bubble, next_bubble);
        next_bubble++;
        if (next_bubble >= BUBBLE_MAX)
        {
            next_bubble = BUBBLE_CYAN;
        }
    }
    else if (key_pressed & PORT_B_KEY_2)
    {
        set_bubble (target_bubble, BUBBLE_NONE);
    }
}


/*
 * Entry point.
 */
void main (void)
{
    /* Setup */
    SMS_setBackdropColor (0);
    SMS_loadBGPalette (background_palette);
    SMS_loadSpritePalette (sprite_palette);
    SMS_useFirstHalfTilesforSprites (false); /* The first half of vram holds the game board */

    /* Patterns 0-319: Game board */
    for (uint16_t i = 0; i < 320; i++)
    {
        UNSAFE_SMS_load1Tile (blue_tile, i);
    }

    /* Patterns 320: Blue tile */
    SMS_loadTiles (blue_tile, 320, sizeof (blue_tile));

    /* Patterns 321: Black tile */
    SMS_loadTiles (black_tile, 321, sizeof (black_tile));

    /* Patterns 322-325: Debug cursor */
    SMS_loadTiles (cursor_patterns, 322, sizeof (cursor_patterns));

    /* Patterns 326-329: Bubble in launcher (sprite) */
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [0] << 3], 326, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [1] << 3], 327, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [2] << 3], 328, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [3] << 3], 329, 32);

    /* Patterns 330-333: Grass */
    SMS_loadTiles (grass_patterns, 330, sizeof (grass_patterns));

    /* Pattern 334: Indicator pip */
    SMS_loadTiles (pip_patterns, 334, sizeof (pip_patterns));

    /* Tile-map: Basic background and border around game board */
    for (uint8_t y = 0; y < 24; y++)
    {
        uint16_t row [32];
        for (uint8_t x = 0; x < 32; x++)
        {
            /* Black border */
            if (x >= 7 && x <= 24 && y == 0 ||
                x == 7 || x == 24)
            {
                row [x] = 321;
            }
            else
            {
                row [x] = 320;
            }

        }
        SMS_loadTileMapArea (0, y, row, 32, 1);
    }

    /* Tile-map: Grass */
    for (uint8_t x = 0; x < 32; x += 2)
    {
        uint16_t grass_block [4] = { 330, 331, 332, 333 };
        SMS_loadTileMapArea (x, 22, grass_block, 2, 2);
    }

    /* Tile-map: Game board strips
     *
     * The game board is organized into 16 vertical strips, each tile in these
     * strips which has a unique pattern associated with it. The bubbles can
     * be freely drawn into these strips without needing to worry about tile
     * boundaries. */
    uint16_t pattern_index = 0;
    for (uint8_t strip = 0; strip < 16; strip++)
    {
        uint16_t strip_map [20];

        for (uint8_t y = 0; y < 20; y++)
        {
            /* Use the sprite palette */
            strip_map [y] = 0x0800 | pattern_index;


            pattern_index += 1;
        }
        SMS_loadTileMapArea (8 + strip, 1, strip_map, 1, 20);
    }

    SMS_displayOn ();

    game_state_t state = BUBBLE_READY;

    while (true)
    {
        SMS_waitForVBlank ();
        uint16_t key_pressed = SMS_getKeysPressed ();
        uint16_t key_status = SMS_getKeysStatus ();

        /* Handle input */
        debug_cursor (key_pressed, key_status);

        /* Aiming */
        uint16_t key_horizontal = key_status & (PORT_A_KEY_LEFT | PORT_A_KEY_RIGHT);
        uint16_t key_vertical = key_status & (PORT_A_KEY_UP | PORT_A_KEY_DOWN);
        if (key_horizontal)
        {
            if (key_horizontal == PORT_A_KEY_LEFT && launcher_aim > LAUNCHER_AIM_MIN)
            {
                launcher_aim -= 1;
            }
            else if (key_horizontal == PORT_A_KEY_RIGHT && launcher_aim < LAUNCHER_AIM_MAX)
            {
                launcher_aim += 1;
            }
        }
        else if (key_vertical)
        {
            if (key_vertical == PORT_A_KEY_UP)
            {
                if (launcher_aim < LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim++;
                }
                else if (launcher_aim > LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim--;
                }
            }
            else if (key_vertical == PORT_A_KEY_DOWN)
            {
                if (launcher_aim > LAUNCHER_AIM_MIN && launcher_aim < LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim--;
                }
                else if (launcher_aim > LAUNCHER_AIM_CENTRE && launcher_aim < LAUNCHER_AIM_MAX)
                {
                    launcher_aim++;
                }
            }
        }

        if (key_pressed & PORT_A_KEY_1)
        {
            if (state == BUBBLE_READY)
            {
                active_bubble_velocity_x = angle_data [launcher_aim].x;
                active_bubble_velocity_y = angle_data [launcher_aim].y;
                state = BUBBLE_MOVING;
            }
        }

        /* Movement */
        if (state == BUBBLE_MOVING)
        {
            active_bubble_x += active_bubble_velocity_x;
            active_bubble_y += active_bubble_velocity_y;

            /* Bounce off the walls.
             * Note, math is simplified and uses modulo 16-bit*/
            /* TODO: Tune the subpixel value to give the half-pixel at the edge like the PS1 version has */
            if (active_bubble_x < LEFT_EDGE)
            {
                active_bubble_x = 0x8000 - active_bubble_x;
                active_bubble_velocity_x = -active_bubble_velocity_x;
            }
            else if (active_bubble_x > RIGHT_EDGE)
            {
                active_bubble_x = 0x6000 - active_bubble_x;
                active_bubble_velocity_x = -active_bubble_velocity_x;
            }



            /* For now, just stop once we reach the end. */
            if (active_bubble_y < 8 * 0x100)
            {
                /* Reset the coordinates for the next bubble */
                active_bubble_x = LAUNCH_FROM_X;
                active_bubble_y = LAUNCH_FROM_Y;
                state = BUBBLE_READY;
            }
        }

        /* Sprites */
        SMS_initSprites ();
        draw_cursor ();
        draw_active_bubble ();
        draw_pip ();
        SMS_copySpritestoSAT ();
    }

}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
