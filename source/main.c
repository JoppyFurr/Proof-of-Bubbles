/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SMSlib.h"

#define TARGET_SMS
#include "../game_tile_data/pattern_index.h"
#include "../game_tile_data/palette.h"

/*
 * Entry point.
 */
void main (void)
{
    /* Setup */
    SMS_setBackdropColor (0);
    SMS_loadBGPalette (background_palette);
    SMS_loadSpritePalette (sprite_palette);
    SMS_useFirstHalfTilesforSprites (true);
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
    }

}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
