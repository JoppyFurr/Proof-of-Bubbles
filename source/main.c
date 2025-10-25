/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SMSlib.h"

#define TARGET_SMS
#include "../game_tile_data/patterns.h"
#include "../game_tile_data/pattern_index.h"
#include "../game_tile_data/palette.h"

/* Global state */
uint8_t cursor_x = 0;
uint8_t cursor_y = 0;

typedef enum bubble_e {
    BUBBLE_NONE = 0,
    BUBBLE_CYAN
} bubble_t;

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

static uint32_t blue_tile [32] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                   0x00000000, 0x00000000, 0x00000000, 0x00000000 };

static uint32_t black_tile [32] = { 0xff0000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                    0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };

void draw_cursor (void)
{
    uint8_t cursor_x_px = 72 + (cursor_x << 4);
    uint8_t cursor_y_px = 16 + (14 * cursor_y);

    /* Staggering */
    if (cursor_y & 0x01)
    {
        cursor_x_px += 8;
    }

    SMS_initSprites ();
    SMS_addSprite (cursor_x_px,     cursor_y_px,     (uint8_t) (322    ));
    SMS_addSprite (cursor_x_px + 8, cursor_y_px,     (uint8_t) (322 + 1));
    SMS_addSprite (cursor_x_px,     cursor_y_px + 8, (uint8_t) (322 + 2));
    SMS_addSprite (cursor_x_px + 8, cursor_y_px + 8, (uint8_t) (322 + 3));
    SMS_copySpritestoSAT ();
}


/*
 * Draw a bubble to the screen at the specified game-board location.
 */
#define BYTES_PER_STRIP 640
#define BYTES_PER_ROW 56
void set_bubble (uint8_t x, uint8_t y, bubble_t bubble)
{
    uint8_t position = 10 + x + 19 * (y >> 1);
    if (y & 1)
    {
        position += NEIGH_BOTTOM_RIGHT;
    }

    game_board [position] = bubble;

    /* Neighbours */
    bubble_t neigh_tl = game_board [position + NEIGH_TOP_LEFT];
    bubble_t neigh_tr = game_board [position + NEIGH_TOP_RIGHT];
    bubble_t neigh_bl = game_board [position + NEIGH_BOTTOM_LEFT];
    bubble_t neigh_br = game_board [position + NEIGH_BOTTOM_RIGHT];

    /* VRAM coordinates */
    uint16_t left_strip;
    uint16_t right_strip;

    /* Odd rows are offset by one strip */
    if (y & 1)
    {
        left_strip = ((x << 1) + 1) * BYTES_PER_STRIP + y * BYTES_PER_ROW;
    }
    else
    {
        left_strip = (x << 1) * BYTES_PER_STRIP + y * BYTES_PER_ROW;
    }
    right_strip = left_strip + BYTES_PER_STRIP;

    if (bubble == BUBBLE_NONE)
    {
        /* Left strip */
        SMS_VRAMmemcpy (left_strip,      &bubbles_patterns [bubbles_panels [0 + neigh_tl] [0] << 3], 32);
        SMS_VRAMmemcpy (left_strip + 32, &bubbles_patterns [bubbles_panels [0 + neigh_bl] [2] << 3], 32);

        /* Right strip */
        SMS_VRAMmemcpy (right_strip,      &bubbles_patterns [bubbles_panels [0 + neigh_tr] [1] << 3], 32);
        SMS_VRAMmemcpy (right_strip + 32, &bubbles_patterns [bubbles_panels [0 + neigh_br] [3] << 3], 32);
    }
    else if (bubble == BUBBLE_CYAN)
    {
        /* To Consider:
         *  - Can the shift be done when the patterns are generated?
         *    As in, give store uint32_t indices rather than pattern indices.
         *  - Cartridge-space is cheap, if the strips are laid out correctly in
         *    the cartridge data, in all bubble-combinations, then two memcpy
         *    could be used instead of four.
         */

        /* Left strip */
        SMS_VRAMmemcpy (left_strip,      &bubbles_patterns [bubbles_panels [2 + neigh_tl] [0] << 3], 32);
        SMS_VRAMmemcpy (left_strip + 32, &bubbles_patterns [bubbles_panels [2 + neigh_bl] [2] << 3], 32);

        /* Right strip */
        SMS_VRAMmemcpy (right_strip,      &bubbles_patterns [bubbles_panels [2 + neigh_tr] [1] << 3], 32);
        SMS_VRAMmemcpy (right_strip + 32, &bubbles_patterns [bubbles_panels [2 + neigh_br] [3] << 3], 32);
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

    while (true)
    {
        SMS_waitForVBlank ();
        uint16_t key_pressed = SMS_getKeysPressed ();

        /* Handle input */
        if ((key_pressed & PORT_A_KEY_UP) && cursor_y > 0)
        {
            cursor_y -= 1;
        }
        else if ((key_pressed & PORT_A_KEY_DOWN) && cursor_y < 10)
        {
            cursor_y += 1;
        }
        else if ((key_pressed & PORT_A_KEY_LEFT) && cursor_x > 0)
        {
            cursor_x -= 1;
        }
        else if ((key_pressed & PORT_A_KEY_RIGHT) && cursor_x < 7)
        {
            cursor_x += 1;
        }

        /* Even rows hold 8 bubbles, odd rows hold 7 bubbles */
        if (cursor_y & 0x01 && cursor_x > 6)
        {
            cursor_x = 6;
        }

        if (key_pressed & PORT_A_KEY_1)
        {
            set_bubble (cursor_x, cursor_y, BUBBLE_CYAN);
        }
        else if (key_pressed & PORT_A_KEY_2)
        {
            set_bubble (cursor_x, cursor_y, BUBBLE_NONE);
        }

        draw_cursor ();
    }

}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
