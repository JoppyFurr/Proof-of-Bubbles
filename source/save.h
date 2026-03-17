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

/* Read a level best time from SRAM. */
void sram_load_best_time (uint8_t level);

/* Write a new best time into SRAM. */
void sram_save_best_time (uint8_t level);
