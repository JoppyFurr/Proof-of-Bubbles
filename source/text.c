/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * Text drawing code.
 */

#include <stdbool.h>
#include <stdint.h>

#include "SMSlib.h"

#include "vram.h"
extern const uint32_t text_patterns [];
extern const uint16_t text_panels [39] [2];


/*
 * Load patterns used for text drawing.
 */
void text_load_patterns (void)
{
    /* "ROUND" */
    for (uint8_t i = 0; i < 5; i++)
    {
        SMS_loadTiles (&text_patterns [text_panels [i + 26] [0] << 3], TEXT_ROUND_PATTERN + i,     32);
        SMS_loadTiles (&text_patterns [text_panels [i + 26] [1] << 3], TEXT_ROUND_PATTERN + i + 5, 32);
    }

    /* "TIME" */
    for (uint8_t i = 0; i < 4; i++)
    {
        SMS_loadTiles (&text_patterns [text_panels [i + 31] [0] << 3], TEXT_TIME_PATTERN + i,     32);
        SMS_loadTiles (&text_patterns [text_panels [i + 31] [1] << 3], TEXT_TIME_PATTERN + i + 4, 32);
    }

    /* "BEST" */
    for (uint8_t i = 0; i < 4; i++)
    {
        SMS_loadTiles (&text_patterns [text_panels [i + 35] [0] << 3], TEXT_BEST_PATTERN + i,     32);
        SMS_loadTiles (&text_patterns [text_panels [i + 35] [1] << 3], TEXT_BEST_PATTERN + i + 4, 32);
    }

    /* Symbols */
    for (uint8_t i = 0; i < 2; i++)
    {
        SMS_loadTiles (&text_patterns [text_panels [i + 10] [0] << 3], SYMBOLS_PATTERN + i,     32);
        SMS_loadTiles (&text_patterns [text_panels [i + 10] [1] << 3], SYMBOLS_PATTERN + i + 2, 32);
    }

    /* Digits */
    for (uint8_t i = 0; i < 10; i++)
    {
        SMS_loadTiles (&text_patterns [text_panels [i] [0] << 3], DIGITS_PATTERN + i,      32);
        SMS_loadTiles (&text_patterns [text_panels [i] [1] << 3], DIGITS_PATTERN + i + 10, 32);
    }
}


/*
 * Draw the round indicator.
 */
void text_draw_round (uint8_t round)
{
    const uint8_t ones = round % 10;
    const uint8_t tens = round / 10;

    /* "ROUND" label - TODO: Usually won't need to be updated. */
    uint16_t label_buf [10] = {
        TEXT_ROUND_PATTERN,     TEXT_ROUND_PATTERN + 1, TEXT_ROUND_PATTERN + 2, TEXT_ROUND_PATTERN + 3, TEXT_ROUND_PATTERN + 4,
        TEXT_ROUND_PATTERN + 5, TEXT_ROUND_PATTERN + 6, TEXT_ROUND_PATTERN + 7, TEXT_ROUND_PATTERN + 8, TEXT_ROUND_PATTERN + 9
    };
    SMS_loadTileMapArea (23, 5, label_buf, 5, 2);

    if (round >= 10)
    {
        /* Tens */
        SMS_loadTiles (&text_patterns [text_panels [13 + tens] [0] << 3], ROUND_DIGITS_PATTERN + 0, 32);
        SMS_loadTiles (&text_patterns [text_panels [13 + tens] [1] << 3], ROUND_DIGITS_PATTERN + 1, 32);
    }
    else
    {
        /* Tens (blank) */
        SMS_loadTiles (&text_patterns [text_panels [12] [0] << 3], ROUND_DIGITS_PATTERN + 0, 32);
        SMS_loadTiles (&text_patterns [text_panels [12] [1] << 3], ROUND_DIGITS_PATTERN + 1, 32);
    }

    /* Ones */
    SMS_loadTiles (&text_patterns [text_panels [13 + ones] [0] << 3], ROUND_DIGITS_PATTERN + 2, 32);
    SMS_loadTiles (&text_patterns [text_panels [13 + ones] [1] << 3], ROUND_DIGITS_PATTERN + 3, 32);

    /* TODO: Name-table usually won't need to be updated. */
    uint16_t digit_buf [10];
    digit_buf [0] = ROUND_DIGITS_PATTERN + 0;
    digit_buf [1] = ROUND_DIGITS_PATTERN + 2;
    digit_buf [2] = ROUND_DIGITS_PATTERN + 1;
    digit_buf [3] = ROUND_DIGITS_PATTERN + 3;
    SMS_loadTileMapArea (29, 5, digit_buf, 2, 2);
}


/*
 * Draw the time indicator
 */
void text_draw_time (void)
{
    /* "TIME" label */
    uint16_t label_buf [8] = {
        TEXT_TIME_PATTERN,     TEXT_TIME_PATTERN + 1, TEXT_TIME_PATTERN + 2, TEXT_TIME_PATTERN + 3,
        TEXT_TIME_PATTERN + 4, TEXT_TIME_PATTERN + 5, TEXT_TIME_PATTERN + 6, TEXT_TIME_PATTERN + 7
    };
    SMS_loadTileMapArea (23, 9, label_buf, 4, 2);

    /* Time digits, hard-coded all zeros for now */
    uint16_t value_buf [16] = {
        DIGITS_PATTERN,      DIGITS_PATTERN,      SYMBOLS_PATTERN,     DIGITS_PATTERN,
        DIGITS_PATTERN,      SYMBOLS_PATTERN + 1, DIGITS_PATTERN,      DIGITS_PATTERN,
        DIGITS_PATTERN + 10, DIGITS_PATTERN + 10, SYMBOLS_PATTERN + 2, DIGITS_PATTERN + 10,
        DIGITS_PATTERN + 10, BLUE_TILE_PATTERN,   DIGITS_PATTERN + 10, DIGITS_PATTERN + 10
    };
    SMS_loadTileMapArea (23, 11, value_buf, 8, 2);
}


/*
 * Draw the best-time indicator
 * Note: for now, hard-coded to display 02:00'00
 */
void text_draw_best (void)
{
    /* "BEST" label */
    uint16_t label_buf [8] = {
        TEXT_BEST_PATTERN,     TEXT_BEST_PATTERN + 1, TEXT_BEST_PATTERN + 2, TEXT_BEST_PATTERN + 3,
        TEXT_BEST_PATTERN + 4, TEXT_BEST_PATTERN + 5, TEXT_BEST_PATTERN + 6, TEXT_BEST_PATTERN + 7
    };
    SMS_loadTileMapArea (23, 14, label_buf, 4, 2);

    /* Best digits, hard-coded two minutes for now */
    uint16_t value_buf [16] = {
        DIGITS_PATTERN,      DIGITS_PATTERN +  2, SYMBOLS_PATTERN,     DIGITS_PATTERN,
        DIGITS_PATTERN,      SYMBOLS_PATTERN + 1, DIGITS_PATTERN,      DIGITS_PATTERN,
        DIGITS_PATTERN + 10, DIGITS_PATTERN + 12, SYMBOLS_PATTERN + 2, DIGITS_PATTERN + 10,
        DIGITS_PATTERN + 10, BLUE_TILE_PATTERN,   DIGITS_PATTERN + 10, DIGITS_PATTERN + 10
    };
    SMS_loadTileMapArea (23, 16, value_buf, 8, 2);
}
