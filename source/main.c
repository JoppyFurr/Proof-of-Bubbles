/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * TODOs:
 *  - Seed rand from R, generate random bubbles.
 *  - Save & restore rand seed.
 *  - Draw the launcher arrow
 *  - Un-crossable line at the bottom of the game-board
 *  - Wash of gray upon crossing the line
 *  - Pop groups of matching bubbles
 *  - Falling animation for dropped bubbles
 *  - Timer & high-score table
 *  - Colourblind mode
 *  - Shaking & dropping
 *  - Music
 */
#include <stdbool.h>
#include <stdint.h>

#include "SMSlib.h"

#include "angles.h"

#define TARGET_SMS
#include "../game_tile_data/patterns.h"
#include "../game_tile_data/pattern_index.h"
#include "../game_tile_data/palette.h"

/* VRAM Locations */
#define ACTIVE_BUBBLE_PATTERN   320
#define PIP_PATTERN             324
#define BLUE_TILE_PATTERN       325
#define GRASS_PATTERN           326
#define BORDER_PATTERN          332

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

typedef enum bubble_e {
    BUBBLE_NONE = 0,
    BUBBLE_CYAN,
    BUBBLE_RED,
    BUBBLE_GREEN,
    BUBBLE_YELLOW,
    BUBBLE_MAX
} bubble_t;

uint8_t launcher_aim = LAUNCHER_AIM_CENTRE;
bubble_t active_bubble = BUBBLE_CYAN;
uint16_t active_bubble_velocity_x = 0;
uint16_t active_bubble_velocity_y = 0;
uint16_t active_bubble_x = LAUNCH_FROM_X;
uint16_t active_bubble_y = LAUNCH_FROM_Y;


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
#define NEIGH_RIGHT         1
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

/* Using the same 16x14 staggered-rectangle grid as above, a bitmap is defined for which
 * bubble-positions are collided with. The coordinates used to index this table is that
 * of the pixel at (7, 7) within the 16 x 16 pixel bubble sprite. */
#define COLLISION_TOP_LEFT      0x01
#define COLLISION_TOP_RIGHT     0x02
#define COLLISION_LEFT          0x04
#define COLLISION_RIGHT         0x08
#define COLLISION_BOTTOM_LEFT   0x10
#define COLLISION_BOTTOM_RIGHT  0x20
static const uint8_t pixel_to_collision [14] [16] = {
    { 0x05, 0x05, 0x07, 0x07, 0x07, 0x07, 0x03, 0x03,  0x03, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a },
    { 0x05, 0x05, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03,  0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a },
    { 0x05, 0x05, 0x05, 0x07, 0x07, 0x07, 0x07, 0x03,  0x0b, 0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x05, 0x05, 0x05, 0x05, 0x07, 0x07, 0x07, 0x03,  0x0b, 0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x05, 0x05, 0x05, 0x05, 0x05, 0x07, 0x07, 0x03,  0x0b, 0x0b, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a },
    { 0x15, 0x15, 0x15, 0x15, 0x15, 0x05, 0x07, 0x03,  0x0b, 0x0a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a },
    { 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x00,  0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a },
    { 0x15, 0x15, 0x15, 0x15, 0x15, 0x14, 0x34, 0x30,  0x38, 0x28, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a },
    { 0x14, 0x14, 0x14, 0x14, 0x14, 0x34, 0x34, 0x30,  0x38, 0x38, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28 },
    { 0x14, 0x14, 0x14, 0x14, 0x34, 0x34, 0x34, 0x30,  0x38, 0x38, 0x38, 0x28, 0x28, 0x28, 0x28, 0x28 },
    { 0x14, 0x14, 0x14, 0x34, 0x34, 0x34, 0x34, 0x30,  0x38, 0x38, 0x38, 0x38, 0x28, 0x28, 0x28, 0x28 },
    { 0x14, 0x14, 0x34, 0x34, 0x34, 0x34, 0x34, 0x30,  0x38, 0x38, 0x38, 0x38, 0x38, 0x28, 0x28, 0x28 },
    { 0x14, 0x14, 0x34, 0x34, 0x34, 0x34, 0x30, 0x30,  0x30, 0x38, 0x38, 0x38, 0x38, 0x28, 0x28, 0x28 },
    { 0x14, 0x34, 0x34, 0x34, 0x34, 0x34, 0x30, 0x30,  0x30, 0x38, 0x38, 0x38, 0x38, 0x38, 0x28, 0x28 }
};


static const uint32_t blue_tile [32] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                         0x00000000, 0x00000000, 0x00000000, 0x00000000 };

static const uint32_t black_tile [32] = { 0xff0000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                          0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };


/*
 * Draw the active bubble, which is either sitting in the launcher,
 * or moving through the game board.
 */
void draw_active_bubble (void)
{
    /* TODO: Union or pointer math to avoid the shifts */
    uint8_t x = active_bubble_x >> 8;
    uint8_t y = active_bubble_y >> 8;
    SMS_addSprite (x,     y,     (uint8_t) (ACTIVE_BUBBLE_PATTERN + 0));
    SMS_addSprite (x + 8, y,     (uint8_t) (ACTIVE_BUBBLE_PATTERN + 1));
    SMS_addSprite (x,     y + 8, (uint8_t) (ACTIVE_BUBBLE_PATTERN + 2));
    SMS_addSprite (x + 8, y + 8, (uint8_t) (ACTIVE_BUBBLE_PATTERN + 3));
}


/*
 * An initial indicator of aiming direction.
 * TODO: Replace with an arrow.
 */
void draw_pip (void)
{
    uint8_t pip_x = 127 + (angle_data [launcher_aim].x >> 6);
    uint8_t pip_y = 161 + (angle_data [launcher_aim].y >> 6);
    SMS_addSprite (pip_x, pip_y, (uint8_t) (PIP_PATTERN));
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
 * Check if the active bubble is currently colliding with
 * one of the bubbles on the game-board.
 */
bool active_bubble_collision (void)
{
    /* Early check to make the ceiling sticky */
    if ((active_bubble_y >> 8) <= 8)
    {
        return true;
    }

    /* Get the bubble-centre coordinate within a two-row block */
    uint8_t pos_x =      ((active_bubble_x >> 8) + 7 - 64);
    uint8_t pos_y =      ((active_bubble_y >> 8) + 7 - 9) % 28;

    /* Get which two-row block the bubble is in */
    uint8_t repetition = ((active_bubble_y >> 8) + 7 - 9) / 28;

    /* Convert the position into a 16x14 staggered-rectangle position on the game-board */
    uint8_t collision_tile = 10 + 19 * repetition;
    uint8_t collision_map;
    if (pos_y < 14) /* 8-bubble row */
    {
        collision_tile += pos_x >> 4;
        collision_map = pixel_to_collision [pos_y] [pos_x & 0x0f];

    }
    else /* 7-bubble row */
    {
        collision_tile += NEIGH_BOTTOM_LEFT;
        collision_tile += (pos_x + 8) >> 4;
        collision_map = pixel_to_collision [pos_y - 14] [(pos_x + 8) & 0x0f];
    }

    /* Note: For now the centre is skipped, assuming a centre collision
     *       would have been detected earlier in the bubble's journey. */

    if (collision_map & COLLISION_TOP_LEFT)
    {
        if (game_board [collision_tile + NEIGH_TOP_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_TOP_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_TOP_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_LEFT)
    {
        if (game_board [collision_tile + NEIGH_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_BOTTOM_LEFT)
    {
        if (game_board [collision_tile + NEIGH_BOTTOM_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_BOTTOM_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_BOTTOM_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }

    return false;
}


/*
 * Set the active bubble on the game-board.
 *
 * TODO: Some of these calculations could be re-used from
 *       the collision-detection above.
 *
 * TODO: Consider setting at the previous frame's position,
 *       before it was inside another bubble.
 */
void active_bubble_set (void)
{
    /* Get the bubble-centre coordinate within a two-row block */
    /* Add an extra +1 to the x position to bias towards rolling right. */
    uint8_t pos_x =      ((active_bubble_x >> 8) + 7 + 1 - 64);
    uint8_t pos_y =      ((active_bubble_y >> 8) + 7 - 9) % 28;

    /* Get which two-row block the bubble is in */
    uint8_t repetition = ((active_bubble_y >> 8) + 7 - 9) / 28;

    /* Convert the position into a 16x14 staggered-rectangle position on the game-board.
     * Then, correct this using the pixel-to-board array, which finds the underlying bubble
     * position as if this were a hex-grid rather than a staggered-rectangle grid. */
    uint8_t bubble_tile = 10 + 19 * repetition;
    if (pos_y < 14) /* 8-bubble row */
    {
        bubble_tile += pos_x >> 4;
        bubble_tile += pixel_to_board [pos_y] [pos_x & 0x0f];

    }
    else /* 7-bubble row */
    {
        bubble_tile += NEIGH_BOTTOM_LEFT;
        bubble_tile += (pos_x + 8) >> 4;
        bubble_tile += pixel_to_board [pos_y - 14] [(pos_x + 8) & 0x0f];
    }

    set_bubble (bubble_tile, active_bubble);
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

    /* Patterns 320-323: Active bubble (sprite) */
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [0] << 3], 320, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [1] << 3], 321, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [2] << 3], 322, 32);
    SMS_loadTiles (&bubbles_patterns [bubbles_panels [BUBBLE_CYAN * BUBBLE_MAX] [3] << 3], 323, 32);

    /* Pattern 324: Indicator pip */
    SMS_loadTiles (pip_patterns, PIP_PATTERN, sizeof (pip_patterns));

    /* Patterns 325: Blue tile */
    SMS_loadTiles (blue_tile, BLUE_TILE_PATTERN, sizeof (blue_tile));

    /* Patterns 326-329: Grass */
    SMS_loadTiles (grass_patterns, GRASS_PATTERN, sizeof (grass_patterns));

    /* Patterns 330-337: Border */
    SMS_loadTiles (border_patterns, BORDER_PATTERN, sizeof (border_patterns));

    /* Tile-map: Background and border around game board */
    for (uint8_t y = 0; y < 24; y++)
    {
        uint16_t row [32];

        /* Blue fill */
        for (uint8_t x = 0; x < 32; x++)
        {
            row [x] = BLUE_TILE_PATTERN;
        }

        /* Wood-texture border */
        if (y == 0) /* First line */
        {
            row [7] = BORDER_PATTERN + 0;
            for (uint8_t x = 8; x < 24; x++)
            {
                row [x] = BORDER_PATTERN + 1;
            }
            row [24] = BORDER_PATTERN + 2;
        }
        else if (y < 22) /* Middle */
        {
            row [7] = BORDER_PATTERN + 3;
            row [24] = BORDER_PATTERN + 5;
            for (uint8_t x = 8; x < 24; x++)
            {
                /* Trying a lighter blue in the game-board, using the sprite
                 * palette. Doesn't reach all the way to the grass though..
                 *
                 * Either the background palette needs to add the light blue,
                 * or the sprite palette needs to have the grass, which would
                 * lock in the green bubble. */
                row [x] = BLUE_TILE_PATTERN | 0x0800;
            }
        }
        else if (y == 22) /* Grass row 1 */
        {
            for (uint8_t x = 0; x < 32; x++)
            {
                row [x] = GRASS_PATTERN + (x & 0x01);
            }
            /* While trying out a lighter blue within the game-board area,
             * special light-blue-sky grass tiles are needed to match. */
            for (uint8_t x = 8; x < 24; x++)
            {
                row [x] += 4;
            }
            row [7] = BORDER_PATTERN + 6;
            row [24] = BORDER_PATTERN + 7;
        }
        else if (y == 23) /* Grass row 2 */
        {
            for (uint8_t x = 0; x < 32; x++)
            {
                row [x] = GRASS_PATTERN + 2 + (x & 0x01);
            }
        }

        SMS_loadTileMapArea (0, y, row, 32, 1);
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

    /* Yellow line */
    const uint32_t yellow_line [2] = { 0x000000ff, 0x0000ff00 };
    uint16_t line_index = 18 * 32 + 4;
    for (uint8_t x = 0; x < 16; x++)
    {
        SMS_VRAMmemcpy (line_index, yellow_line, 8);
        line_index += 640;
    }

    SMS_displayOn ();

    game_state_t state = BUBBLE_READY;

    while (true)
    {
        SMS_waitForVBlank ();
        uint16_t key_pressed = SMS_getKeysPressed ();
        uint16_t key_status = SMS_getKeysStatus ();

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
            /* Placing the collision-check here rather than after the velocity
             * is applied, allows the bubble to enjoy a frame of visible collision.
             * This seems to be what happens on the PS1 version.
             *
             * Revisit this decision once a couple of extra frames are added to
             * smooth the transition from the overlap-frame to the final position.
             * If it still looks kinda choppy, then maybe remove the frame of
             * overlap. */
            if (active_bubble_collision ())
            {
                active_bubble_set ();

                /* Reset the coordinates for the next bubble */
                active_bubble_x = LAUNCH_FROM_X;
                active_bubble_y = LAUNCH_FROM_Y;
                active_bubble += 1;
                if (active_bubble >= BUBBLE_MAX)
                {
                    active_bubble = BUBBLE_CYAN;
                }

                SMS_loadTiles (&bubbles_patterns [bubbles_panels [active_bubble * BUBBLE_MAX] [0] << 3],
                               ACTIVE_BUBBLE_PATTERN + 0, 32);
                SMS_loadTiles (&bubbles_patterns [bubbles_panels [active_bubble * BUBBLE_MAX] [1] << 3],
                               ACTIVE_BUBBLE_PATTERN + 1, 32);
                SMS_loadTiles (&bubbles_patterns [bubbles_panels [active_bubble * BUBBLE_MAX] [2] << 3],
                               ACTIVE_BUBBLE_PATTERN + 2, 32);
                SMS_loadTiles (&bubbles_patterns [bubbles_panels [active_bubble * BUBBLE_MAX] [3] << 3],
                               ACTIVE_BUBBLE_PATTERN + 3, 32);

                state = BUBBLE_READY;
            }
            else
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
            }
        }

        /* Sprites */
        SMS_initSprites ();
        draw_active_bubble ();
        draw_pip ();
        SMS_copySpritestoSAT ();
    }

}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
