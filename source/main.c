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
 * Entry point.
 */
void main (void)
{
    /* Setup */
    SMS_setBackdropColor (0);
    SMS_loadBGPalette (background_palette);
    SMS_loadSpritePalette (sprite_palette);
    SMS_useFirstHalfTilesforSprites (false); /* The first half of vram holds the game board */
    SMS_displayOn ();

    /* TODO: Patterns 0-319: Game board */

    /* Patterns 320: Blue tile */
    uint32_t blue_tile [32] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                0x00000000, 0x00000000, 0x00000000, 0x00000000 };
    SMS_loadTiles (blue_tile, 320, sizeof (blue_tile));

    /* Patterns 321: Black tile */
    uint32_t black_tile [32] = { 0xff0000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                 0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };
    SMS_loadTiles (black_tile, 321, sizeof (black_tile));

    /* Patterns 322-325: Debug cursor */
    SMS_loadTiles (cursor_patterns, 322, sizeof (cursor_patterns));

    /* Draw basic game screen */
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

        draw_cursor ();
    }

}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
