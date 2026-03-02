/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * SRAM Save Implementation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SMSlib.h"


/* The magic string holds the date of the most recent change to what
 * the save data represents. If something changes to invalidate the
 * save data (eg, deleting or replacing levels, adding new settings),
 * then date should be updated so that the previously saved data is
 * not used. */
char *sram_magic = "SNEP-2026-03-02";


/*
 * Read SRAM data and use if valid.
 */
void sram_load (void)
{
    SMS_enableSRAM ();

    /* Offset 0x00: Magic string */
    if (memcmp (sram_magic, &SMS_SRAM [0], 16) == 0)
    {
        /* Offset 0x10: RNG state */
        srand (*(uint16_t *)(&SMS_SRAM [0x10]));
    }

    SMS_disableSRAM ();
}


/*
 * Write saved settings into SRAM.
 */
void sram_save (void)
{
    SMS_enableSRAM ();

    /* Offset 0x00: Magic string */
    memcpy (&SMS_SRAM [0], sram_magic, 16);

    /* Offset 0x10: RNG state */
    (* (uint16_t *)(&SMS_SRAM [0x10])) = rand ();

    SMS_disableSRAM ();
}
