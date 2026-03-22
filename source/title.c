/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * Title screen implementation
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SMSlib.h"

#define TARGET_SMS
#include "bank_4.h"
#include "../title_tile_data/palette.h"
#include "../title_tile_data/pattern_index.h"

#include "title.h"

/*
 * Run the title screen.
 */
void title_screen (void)
{
    SMS_displayOff ();
    SMS_loadBGPalette (background_palette);
    SMS_loadSpritePalette (background_palette);
    SMS_setBackdropColor (0);

    SMS_mapROMBank (4); /* Title screen patterns */
    SMS_loadTiles (title_screen_patterns, 0, sizeof (title_screen_patterns));
    SMS_mapROMBank  (2);

    SMS_loadTileMapArea (0, 0, title_screen_indices, 32, 24);

    /* Re-write the "PRESS START" name-table entries to
     * use the sprite palette to allow a colour cycle. */
    uint16_t press_start [28];
    memcpy (press_start, &title_screen_indices [457], 14 * sizeof (uint16_t));
    memcpy (&press_start [14], &title_screen_indices [489], 14 * sizeof (uint16_t));
    for (uint8_t i = 0; i < 28; i++)
    {
        press_start [i] |= 0x0800;
    }
    SMS_loadTileMapArea (9, 14, press_start, 14, 2);

    SMS_displayOn ();

    int frame = 0;
    int cycle_step = 0;

    uint8_t cycle_data [32] = {
         3,  3,  7, 11, 15, 15, 14, 13,
        12, 12, 24, 36, 52, 52, 53, 54,
        50, 50, 54, 58, 63, 63, 63, 59,
        59, 59, 58, 58, 61, 61, 43, 23
    };

    while (true)
    {
        uint16_t key_pressed = SMS_getKeysPressed ();

        if (key_pressed & (PORT_A_KEY_1 | PORT_A_KEY_2))
        {
            break;
        }

        frame += 1;

        if (frame >= 4)
        {
            SMS_setSpritePaletteColor ( 1, cycle_data [(cycle_step +  0) & 31]);
            SMS_setSpritePaletteColor ( 2, cycle_data [(cycle_step +  1) & 31]);
            SMS_setSpritePaletteColor ( 3, cycle_data [(cycle_step +  2) & 31]);
            SMS_setSpritePaletteColor ( 4, cycle_data [(cycle_step +  3) & 31]);
            SMS_setSpritePaletteColor ( 5, cycle_data [(cycle_step +  4) & 31]);
            SMS_setSpritePaletteColor ( 6, cycle_data [(cycle_step +  5) & 31]);
            SMS_setSpritePaletteColor ( 7, cycle_data [(cycle_step +  6) & 31]);
            SMS_setSpritePaletteColor ( 8, cycle_data [(cycle_step +  7) & 31]);
            SMS_setSpritePaletteColor ( 9, cycle_data [(cycle_step +  8) & 31]);
            SMS_setSpritePaletteColor (10, cycle_data [(cycle_step +  9) & 31]);
            SMS_setSpritePaletteColor (11, cycle_data [(cycle_step + 10) & 31]);

            cycle_step += 1;
            frame = 0;
        }

        SMS_waitForVBlank ();
    }


    SMS_displayOff ();
}

