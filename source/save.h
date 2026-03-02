/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * SRAM Save Header
 */

/* Read SRAM data and use if valid. */
void sram_load (void);

/* Write saved settings into SRAM. */
void sram_save (void);
