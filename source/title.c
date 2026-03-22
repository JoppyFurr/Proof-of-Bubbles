/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * Title screen implementation
 */

#include <stdbool.h>
#include <stdint.h>

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

    SMS_displayOn ();

    while (true)
    {
        uint16_t key_pressed = SMS_getKeysPressed ();

        if (key_pressed & (PORT_A_KEY_1 | PORT_A_KEY_2))
        {
            break;
        }

        SMS_waitForVBlank ();
    }


    SMS_displayOff ();
}

