/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * Locations of patterns in vram.
 *
 * Note:
 *     [  0 - 447] Patterns
 *     [448 - 495] Name table (no vertical scroll)
 *     [496 - 503] Patterns
 *     [504 - 511] Sprite table
 *
 *     ** Currently only 13 patterns unused **
 *     Features requiring additional patterns should be done first or removed.
 */

/* VRAM Locations */
#define BUBBLE_PATTERN          320
#define PIP_PATTERN             356
#define BLUE_TILE_PATTERN       357
#define GRASS_PATTERN           358
#define NEXT_BUBBLE_PATTERN     366
#define BORDER_PATTERN          370
#define TEXT_ROUND_PATTERN      378
#define TEXT_TIME_PATTERN       388
#define TEXT_BEST_PATTERN       396
#define SYMBOLS_PATTERN         404
#define ROUND_DIGITS_PATTERN    407
#define DIGITS_PATTERN          411
#define VRAM_END                443
