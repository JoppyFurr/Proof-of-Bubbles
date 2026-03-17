/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * SRAM Save Implementation
 *
 * 0x00: Magic string (16 bytes)
 * 0x10: RNG crumb (2 bytes)
 *
 * 0x100: Best Times (40 bytes)
 *
 *        For now, use four bytes per level:
 *
 *          [3] unused
 *          [2] minutes
 *          [1] seconds
 *          [0] frames
 *
 *  Feeps:
 *   - Consider storing the top three times, instead of just the one.
 *   - Consider allowing a 3-letter name to be saved with each time.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SMSlib.h"
#include "save.h"

/* The magic string holds the date of the most recent change to what
 * the save data represents. If something changes to invalidate the
 * save data (eg, deleting or replacing levels, adding new settings),
 * then date should be updated so that the previously saved data is
 * not used. */
char *sram_magic = "SNEP-2026-03-17";

/* Directly access the best-time variables when saving and loading */
extern uint8_t best_time_minutes;
extern uint8_t best_time_seconds;
extern uint8_t best_time_frames;


/*
 * Read SRAM data and use if valid.
 */
void sram_load (void)
{
    bool invalid = false;
    SMS_enableSRAM ();

    /* Offset 0x00: Magic string */
    if (memcmp (sram_magic, &SMS_SRAM [0], 16) == 0)
    {
        /* Offset 0x10: RNG state */
        srand (*(uint16_t *)(&SMS_SRAM [0x10]));
    }
    else
    {
        invalid = true;
    }

    SMS_disableSRAM ();

    /* If the SRAM data is not a valid save, save
     * now to initialise the best time data. */
    if (invalid)
    {
        sram_save ();
    }

}


/*
 * Write saved settings into SRAM.
 */
void sram_save (void)
{
    SMS_enableSRAM ();

    /* Check if data needs to be initialised */
    if (memcmp (sram_magic, &SMS_SRAM [0], 16) != 0)
    {
        /* Offset 0x00: Magic string */
        memcpy (&SMS_SRAM [0], sram_magic, 16);

        /* Offset 0x100: Best times */
        for (uint8_t level = 0; level < 10; level++)
        {
            /* Initialise best time to 2:00:00 */
            SMS_SRAM [0x100 + (level << 2) + 0] = 0;
            SMS_SRAM [0x100 + (level << 2) + 1] = 0;
            SMS_SRAM [0x100 + (level << 2) + 2] = 2;
        }
    }

    /* Offset 0x10: RNG crumb */
    (* (uint16_t *)(&SMS_SRAM [0x10])) = rand ();

    SMS_disableSRAM ();
}


/*
 * Read a level best time from SRAM.
 */
void sram_load_best_time (uint8_t level)
{
    SMS_enableSRAM ();

    /* Offset 0x00: Magic string */
    if (memcmp (sram_magic, &SMS_SRAM [0], 16) == 0)
    {
        best_time_frames = SMS_SRAM [0x100 + (level << 2) + 0];
        best_time_seconds = SMS_SRAM [0x100 + (level << 2) + 1];
        best_time_minutes = SMS_SRAM [0x100 + (level << 2) + 2];
    }
    else
    {
        best_time_frames = 0;
        best_time_seconds = 0;
        best_time_minutes = 2;
    }

    SMS_disableSRAM ();
}


/*
 * Write a new best time into SRAM.
 */
void sram_save_best_time (uint8_t level)
{
    SMS_enableSRAM ();

    /* Offset 0x00: Magic string */
    if (memcmp (sram_magic, &SMS_SRAM [0], 16) == 0)
    {
        SMS_SRAM [0x100 + (level << 2) + 0] = best_time_frames;
        SMS_SRAM [0x100 + (level << 2) + 1] = best_time_seconds;
        SMS_SRAM [0x100 + (level << 2) + 2] = best_time_minutes;
    }

    SMS_disableSRAM ();
}
