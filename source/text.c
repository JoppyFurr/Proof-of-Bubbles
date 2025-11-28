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
}


/*
 * Draw the round indicator.
 */
void text_draw_round (uint8_t round)
{
    uint16_t text_buf [10];

    const uint8_t ones = round % 10;
    const uint8_t tens = round / 10;

    /* "ROUND" label - TODO: Usually won't need to be updated. */
    for (uint8_t i = 0; i < 5; i++)
    {
        text_buf [i    ] = TEXT_ROUND_PATTERN + i;
        text_buf [i + 5] = TEXT_ROUND_PATTERN + i + 5;
    }
    SMS_loadTileMapArea (23, 5, text_buf, 5, 2);

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
    text_buf [0] = ROUND_DIGITS_PATTERN + 0;
    text_buf [1] = ROUND_DIGITS_PATTERN + 2;
    text_buf [2] = ROUND_DIGITS_PATTERN + 1;
    text_buf [3] = ROUND_DIGITS_PATTERN + 3;
    SMS_loadTileMapArea (29, 5, text_buf, 2, 2);
}
