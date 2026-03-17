/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * Text drawing header.
 */

/* Load patterns used for text drawing. */
void text_load_patterns (void);

/* Draw the round indicator. */
void text_draw_round (uint8_t round);

/* Update the time indicator */
void text_update_time (void);

/* Draw the time indicator */
void text_draw_time (void);

/* Draw the best-time indicator */
void text_draw_best (void);

/* Update the best time indicator */
void text_update_best (void);
