/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * TODOs:
 *  - Seed rand from R
 *  - Save & restore rand seed.
 *  - Draw the launcher arrow
 *  - High-score table
 *  - Colourblind mode
 *  - Shaking & dropping
 *  - Music
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "SMSlib.h"
#include "PSGlib.h"

#include "vram.h"
#include "data.h"
#include "level_data.h"
#include "sound_data.h"
#include "text.h"

#define TARGET_SMS
#include "bank_2.h"
#include "bank_3.h"
#include "../game_tile_data/palette.h"
#include "../game_tile_data/pattern_index.h"
#include "../bubble_tile_data/pattern_index.h"

#define LAUNCHER_AIM_MIN          0
#define LAUNCHER_AIM_CENTRE      60
#define LAUNCHER_AIM_MAX        120

/* Coordinates in 8.8 fixed-point format */
#define LEFT_EDGE        0x2800
#define RIGHT_EDGE       0x9800
#define LAUNCH_FROM_X    0x6000
#define LAUNCH_FROM_Y    0x9a00

typedef enum bubble_e {
    BUBBLE_NONE = 0,
    BUBBLE_CYAN,
    BUBBLE_RED,
    BUBBLE_GREEN,
    BUBBLE_YELLOW,
    BUBBLE_PURPLE,
    BUBBLE_ORANGE,
    BUBBLE_BLACK,
    BUBBLE_WHITE,
    BUBBLE_CLEAR,
    BUBBLE_MAX
} bubble_t;

#define BUBBLE_INVALID 0x80

uint8_t launcher_aim = LAUNCHER_AIM_CENTRE;
uint8_t tick_holdoff = 0;

/* The "active bubble" is the single bubble that is currently:
 *  - Loaded into the bubble-launcher, or
 *  - In flight, or
 *  - In the process of landing.
 * A lot of functionality interacts with the active bubble, so
 * the state is made global for quick access. */
static bubble_t next_bubble = BUBBLE_CYAN;
static bubble_t active_bubble_colour = BUBBLE_CYAN;
static uint16_t active_bubble_velocity_x = 0;
static uint16_t active_bubble_velocity_y = 0;
static uint16_t active_bubble_x = LAUNCH_FROM_X;
static uint16_t active_bubble_y = LAUNCH_FROM_Y;
static uint8_t active_bubble_board_position = 0;

typedef enum game_state_e {
    BUBBLE_READY = 0,
    BUBBLE_MOVING,
    BUBBLE_LANDED,
    ROUND_IS_LOST,
    ROUND_IS_WON,
} game_state_t;

game_state_t state = BUBBLE_READY;

uint8_t time_minutes = 0;
uint8_t time_seconds = 0;
uint8_t time_frames = 0;

#define GREY_WASH_BEGIN 43
#define GREY_WASH_COMPLETE 0xff
uint8_t grey_wash_step = GREY_WASH_COMPLETE;

#define BOARD_ROWS 11
#define BOARD_COLS 8

/* Using a linear game-board, neighbouring bubbles can be checked with
 * simple offsets. An border of unset bubbles is included in the array
 * to avoid the need for bounds-checking.
 *
 *  -> 6 rows have 8 bubbles (8 rows of 10 bubbles considering the border)
 *  -> 5 rows have 7 bubbles (7 rows of 9 bubbles considering the border)
 *
 *  8 * 10 + 7 * 9 = 161 positions in the array.
 *
 * 'game_board' is the logical game board, containing bubbles that are currently in-play.
 *              This is used to check groups of matching bubbles, check for detached
 *              bubbles that need to drop, and to check the win condition.
 *
 * 'game_board_visible' represents the bitmap graphics currently displayed, including
 *                      out-of-play bubbles waiting for their fall animation to start.
 *                      This allows drawing of correct neighbouring pixels around bubbles.
 */
bubble_t game_board [143];
bubble_t game_board_visible [143];
#define NEIGH_TOP_LEFT    -10
#define NEIGH_TOP_RIGHT    -9
#define NEIGH_LEFT         -1
#define NEIGH_RIGHT         1
#define NEIGH_BOTTOM_LEFT   9
#define NEIGH_BOTTOM_RIGHT 10
static int8_t neighbours [6] = { NEIGH_TOP_LEFT, NEIGH_TOP_RIGHT,   NEIGH_LEFT,
                                 NEIGH_RIGHT,    NEIGH_BOTTOM_LEFT, NEIGH_BOTTOM_RIGHT };

uint8_t colour_count [BUBBLE_MAX] = { 0 };

/* A duplicate table sharing the same coordinate system as the game board.
 * Used when checking for matching groups of bubbles. Note: This could could be
 * made smaller to exclude borders and the row that is already across the line. */
uint8_t match_map [143];
#define MATCH_UNCHECKED     0
#define MATCH_QUEUED        1
#define MATCH_CONFIRMED     2

/* Similar to above, a duplicate table sharing the same coordinate system as
 * the game board. But this time, it's for determining if bubbles are connected
 * to the top of the game-board. Any bubbles that are not connected need to drop. */
uint8_t float_map [143];
#define FLOAT_UNCHECKED     0
#define FLOAT_QUEUED        1
#define FLOAT_CONNECTED     2

static const uint32_t blue_tile [32] = { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
                                         0x00000000, 0x00000000, 0x00000000, 0x00000000 };

static const uint32_t black_tile [32] = { 0xff0000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                          0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff };


/*
 * Draw the active bubble, which is either sitting in the launcher,
 * or moving through the game board.
 */
void draw_active_bubble (void)
{
    uint8_t pattern_index = BUBBLE_PATTERN + ((active_bubble_colour - 1) << 2);

    /* TODO: Union or pointer maths to avoid the shifts */
    uint8_t x = active_bubble_x >> 8;
    uint8_t y = active_bubble_y >> 8;
    SMS_addSprite (x,     y,     (uint8_t) (pattern_index    ));
    SMS_addSprite (x + 8, y,     (uint8_t) (pattern_index + 1));
    SMS_addSprite (x,     y + 8, (uint8_t) (pattern_index + 2));
    SMS_addSprite (x + 8, y + 8, (uint8_t) (pattern_index + 3));
}


/*
 * An initial indicator of aiming direction.
 * TODO: Replace with an arrow.
 */
void draw_pip (void)
{
    uint8_t pip_x = 103 + (angle_data [launcher_aim].x >> 6);
    uint8_t pip_y = 161 + (angle_data [launcher_aim].y >> 6);
    SMS_addSprite (pip_x, pip_y, (uint8_t) (PIP_PATTERN));
}


/*
 * Draw a bubble to the screen at the specified game-board location.
 */
#define BYTES_PER_STRIP 640
#define BYTES_PER_ROW 56
void draw_bubble (uint8_t position, bubble_t bubble)
{
    /* Nothing to do if the bubble has already been drawn. */
    if (game_board_visible [position] == bubble)
    {
        return;
    }

    game_board_visible [position] = bubble;

    /* Neighbours, masking out invalid neighbours. */
    bubble_t neigh_tl = game_board_visible [position + NEIGH_TOP_LEFT] & 0x7f;
    bubble_t neigh_tr = game_board_visible [position + NEIGH_TOP_RIGHT] & 0x7f;
    bubble_t neigh_bl = game_board_visible [position + NEIGH_BOTTOM_LEFT] & 0x7f;
    bubble_t neigh_br = game_board_visible [position + NEIGH_BOTTOM_RIGHT] & 0x7f;

    /* VRAM coordinates */
    uint16_t left_strip;
    uint16_t right_strip;

    uint8_t col = (position - 10) % 19;
    uint8_t row = (position - 10) / 19 * 2;

    /* Odd rows are offset by one strip */
    if (col > 8)
    {
        col -= NEIGH_BOTTOM_RIGHT;
        row += 1;
        left_strip = ((col << 1) + 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    else
    {
        left_strip = (col << 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    right_strip = left_strip + BYTES_PER_STRIP;

    /* Bubble panels are laid out in a square grid.
     *  - Each row contains the bubble to draw {none, cyan, red, green}
     *  - Each column contains the specific neighbour {none, cyan, red, green}
     */

    /* TODO - Can the below be improved a bit?
     * Consider:
     *  - Avoid the left-shift by having Sneptile provide uint32_t indices instead of pattern indices.
     */

    SMS_mapROMBank (3); /* Bubbles */

    /* Left strip */
    SMS_VRAMmemcpy (left_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tl] [0] << 3], 32);
    SMS_VRAMmemcpy (left_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_bl] [2] << 3], 32);

    /* Right strip */
    SMS_VRAMmemcpy (right_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tr] [1] << 3], 32);
    SMS_VRAMmemcpy (right_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_br] [3] << 3], 32);

    SMS_mapROMBank (2);
}


/*
 * Set a bubble's presence in the logical game_board.
 */
void set_bubble (uint8_t position, bubble_t bubble)
{
    /* Keep track of bubble counts, to work out valid
     * next bubbles to put into the launcher. */
    colour_count [game_board [position]] -= 1;
    colour_count [bubble] += 1;

    game_board [position] = bubble;
}


/*
 * Convert a half-bubble to grey.
 */
void set_halfbubble_grey (uint8_t position, bool top_half)
{
    bubble_t bubble = game_board_visible [position];

    if (bubble == BUBBLE_NONE)
    {
        return;
    }

    uint8_t col = (position - 10) % 19;
    uint8_t row = (position - 10) / 19 * 2;

    uint16_t left_strip;
    uint16_t right_strip;

    /* TODO: Macro or lookup-table rather than duplicating the maths? */
    /* Odd rows are offset by one strip */
    if (col > 8)
    {
        col -= NEIGH_BOTTOM_RIGHT;
        row += 1;
        left_strip = ((col << 1) + 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    else
    {
        left_strip = (col << 1) * BYTES_PER_STRIP + row * BYTES_PER_ROW;
    }
    right_strip = left_strip + BYTES_PER_STRIP;

    if (top_half)
    {
        bubble_t neigh_tl = game_board [position + NEIGH_TOP_LEFT];
        bubble_t neigh_tr = game_board [position + NEIGH_TOP_RIGHT];

        SMS_VRAMmemcpy (left_strip,       &bubbles_grey_patterns [bubbles_grey_panels [bubble * BUBBLE_MAX + neigh_tl] [0] << 3], 32);
        SMS_VRAMmemcpy (right_strip,      &bubbles_grey_patterns [bubbles_grey_panels [bubble * BUBBLE_MAX + neigh_tr] [1] << 3], 32);
    }
    else
    {
        bubble_t neigh_bl = game_board [position + NEIGH_BOTTOM_LEFT];
        bubble_t neigh_br = game_board [position + NEIGH_BOTTOM_RIGHT];
        SMS_VRAMmemcpy (left_strip  + 32, &bubbles_grey_patterns [bubbles_grey_panels [bubble * BUBBLE_MAX + neigh_bl] [2] << 3], 32);
        SMS_VRAMmemcpy (right_strip + 32, &bubbles_grey_patterns [bubbles_grey_panels [bubble * BUBBLE_MAX + neigh_br] [3] << 3], 32);
    }
}


/*
 * Upon crossing the line, convert all bubbles to grey.
 *
 * Note: The bits of grey_wash_step is split into three parts:
 *       { row, upper_or_lower_half, delay }
 */
void wash_bubbles_grey (void)
{
    if (grey_wash_step == 0xff)
    {
        return;
    }

    /* Update the wash-to-grey animation only every second frame */
    if (grey_wash_step & 0x01)
    {
        uint8_t row = grey_wash_step >> 2;
        bool top_half = ~grey_wash_step & 0x02;

        uint8_t row_length = (row & 0x01) ? 7 : 8;

        for (uint8_t i = 0; i < row_length; i++)
        {
            set_halfbubble_grey (row_first_bubble [row] + i, top_half);

        }
    }

    /* Mark game_board_visible as invalid. */
    if (grey_wash_step == 0)
    {
        for (uint8_t position = 10; position < 114; position++)
        {
            if (game_board_visible [position] != BUBBLE_NONE)
            {
                game_board_visible [position] = BUBBLE_INVALID;
            }
        }
    }

    grey_wash_step -= 1;
}


/*
 * Check if the active bubble is currently colliding with
 * one of the bubbles on the game-board.
 */
bool active_bubble_collision (void)
{
    /* Early check to make the ceiling sticky */
    if ((active_bubble_y >> 8) <= 8)
    {
        return true;
    }

    /* Get the bubble-centre coordinate within a two-row block */
    uint8_t pos_x =      ((active_bubble_x >> 8) + 7 - 40);
    uint8_t pos_y =      ((active_bubble_y >> 8) + 7 - 9) % 28;

    /* Get which two-row block the bubble is in */
    uint8_t repetition = ((active_bubble_y >> 8) + 7 - 9) / 28;

    /* Convert the position into a 16x14 staggered-rectangle position on the game-board */
    uint8_t collision_tile = 10 + 19 * repetition;
    uint8_t collision_map;
    if (pos_y < 14) /* 8-bubble row */
    {
        collision_tile += pos_x >> 4;
        collision_map = pixel_to_collision [pos_y] [pos_x & 0x0f];

    }
    else /* 7-bubble row */
    {
        collision_tile += NEIGH_BOTTOM_LEFT;
        collision_tile += (pos_x + 8) >> 4;
        collision_map = pixel_to_collision [pos_y - 14] [(pos_x + 8) & 0x0f];
    }

    /* Note: For now the centre is skipped, assuming a centre collision
     *       would have been detected earlier in the bubble's journey. */

    if (collision_map & COLLISION_TOP_LEFT)
    {
        if (game_board [collision_tile + NEIGH_TOP_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_TOP_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_TOP_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_LEFT)
    {
        if (game_board [collision_tile + NEIGH_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_BOTTOM_LEFT)
    {
        if (game_board [collision_tile + NEIGH_BOTTOM_LEFT] != BUBBLE_NONE)
        {
            return true;
        }
    }
    if (collision_map & COLLISION_BOTTOM_RIGHT)
    {
        if (game_board [collision_tile + NEIGH_BOTTOM_RIGHT] != BUBBLE_NONE)
        {
            return true;
        }
    }

    return false;
}


/*
 * Get the active bubble's position on the game-board.
 *
 * TODO: Some of these calculations could be re-used from
 *       the collision-detection above.
 *
 * TODO: Consider setting at the previous frame's position,
 *       before it was inside another bubble.
 */
void active_bubble_calculate_board_position (void)
{
    /* Get the bubble-centre coordinate within a two-row block */
    /* Add an extra +1 to the x position to bias towards rolling right. */
    uint8_t pos_x =      ((active_bubble_x >> 8) + 7 + 1 - 40);
    uint8_t pos_y =      ((active_bubble_y >> 8) + 7 - 9) % 28;

    /* Get which two-row block the bubble is in */
    uint8_t repetition = ((active_bubble_y >> 8) + 7 - 9) / 28;

    /* Convert the position into a 16x14 staggered-rectangle position on the game-board.
     * Then, correct this using the pixel-to-board array, which finds the underlying bubble
     * position as if this were a hex-grid rather than a staggered-rectangle grid. */
    uint8_t bubble_tile = 10 + 19 * repetition;
    if (pos_y < 14) /* 8-bubble row */
    {
        bubble_tile += pos_x >> 4;
        bubble_tile += pixel_to_board [pos_y] [pos_x & 0x0f];

    }
    else /* 7-bubble row */
    {
        bubble_tile += NEIGH_BOTTOM_LEFT;
        bubble_tile += (pos_x + 8) >> 4;
        bubble_tile += pixel_to_board [pos_y - 14] [(pos_x + 8) & 0x0f];
    }

    active_bubble_board_position = bubble_tile;
}


/*
 * Check if the active bubble, having just
 * landed, has formed a group of three to pop.
 */
bool active_bubble_try_pop (void)
{
    /* TODO: Rather than clearing the match-map just before use, it could
     *       be marked dirty, to be automatically zeroed when some spare
     *       time is available. Eg, during the VDP active-area period. */
    memset (match_map, MATCH_UNCHECKED, sizeof (match_map));

    uint8_t stack [80];
    uint8_t stack_size = 0;
    uint8_t match_count = 0;

    /* The stack contains matching bubbles to explore */
    stack [stack_size++] = active_bubble_board_position;

    while (stack_size > 0)
    {
        uint8_t match_pos = stack [--stack_size];

        /* Skip matching bubbles that have already been explored. */
        if (match_map [match_pos] == MATCH_CONFIRMED)
        {
            continue;
        }

        /* Add each newly-found matching neighbour to the stack */
        for (uint8_t n = 0; n < 6; n++)
        {
            uint8_t neighbour = match_pos + neighbours [n];

            if (match_map [neighbour] == MATCH_UNCHECKED &&
                game_board [neighbour] == active_bubble_colour)
            {
                stack [stack_size++] = neighbour;
                match_map [neighbour] = MATCH_QUEUED;
            }
        }

        /* Mark this bubble as having been explored. */
        match_map [match_pos] = MATCH_CONFIRMED;
        match_count++;
    }

    if (match_count >= 3)
    {
        /* Clear each of the matching bubbles */
        /* NOTE: Could either iterate over the map, or
         *       could maintain a list, to avoid the 100+
         *       item iteration. */
        for (uint8_t i = 10; i <= 102; i++)
        {
            if (match_map [i] == MATCH_CONFIRMED)
            {
                set_bubble (i, BUBBLE_NONE);
                draw_bubble (i, BUBBLE_NONE);
            }
        }
        return true;
    }
    return false;
}


/*
 * Update the positions of currently falling bubbles.
 */
uint8_t fall_queue [64];
uint8_t fall_queue_head = 0;
uint8_t fall_queue_tail = 0;

typedef struct falling_bubble_s {
    uint8_t pattern;
    uint8_t x;
    uint8_t y;
    uint8_t velocity;
    uint8_t frame;
} falling_bubble_t;

falling_bubble_t currently_falling [4];

uint8_t currently_falling_count = 0;
uint8_t currently_falling_head = 0;
uint8_t currently_falling_tail = 0;

uint8_t last_drop_began = 0;

static void draw_fallers (void)
{
    /* To avoid too many sprites on the same line, only begin a
     * drop if 8 frames have passed since the previous drop began. */
    if (last_drop_began < 8)
    {
        last_drop_began++;
    }

    /* Note: Because the bottom row falls first & accelerates, it's done first.
     *       So, a ring may be the way to implement the currently-falling list.
     *       With a power-of-2 size, the modulus is just &. */

    /* If we're not at the limit of simultaneously falling bubbles, and there
     * are more waiting to fall, take one from the queue. */
    if (last_drop_began >= 8 && currently_falling_count < 4 && fall_queue_head != fall_queue_tail)
    {
        /* add a bubble from the fall-queue into currently-falling */
        uint8_t position = fall_queue [fall_queue_head++ & 0x3f];
        bubble_t type = game_board_visible [position];
        falling_bubble_t *new = &currently_falling [currently_falling_tail++ & 0x03];

        new->pattern = BUBBLE_PATTERN + ((type - 1) << 2);
        new->x = game_board_x [position];
        new->y = game_board_y [position];
        new->velocity = 1;
        new->frame = 0;

        /* Replace the falling bubble with whatever currently occupies the logical
         * game_board position, usually BUBBLE_NONE. */
        draw_bubble (position, game_board [position]);
        currently_falling_count += 1;
        last_drop_began = 0;
    }

    uint8_t completed = 0;
    for (uint8_t i = currently_falling_head; i != currently_falling_tail; i++)
    {
        falling_bubble_t *bubble = &currently_falling [i & 0x03];

        /* Accelerate every five frames */
        if (bubble->frame == 5)
        {
            bubble->velocity += 1;
            bubble->frame = 0;
        }
        bubble->y += bubble->velocity;
        bubble->frame += 1;

        /* If the bubble is still on-screen */
        if (bubble->y < 200)
        {
            SMS_addSprite (bubble->x,     bubble->y,     (uint8_t) (bubble->pattern    ));
            SMS_addSprite (bubble->x + 8, bubble->y,     (uint8_t) (bubble->pattern + 1));
            SMS_addSprite (bubble->x,     bubble->y + 8, (uint8_t) (bubble->pattern + 2));
            SMS_addSprite (bubble->x + 8, bubble->y + 8, (uint8_t) (bubble->pattern + 3));
        }
        else
        {
            /* Remove from the currently-falling list */
            currently_falling_head += 1;
            currently_falling_count -= 1;
        }
    }

    currently_falling_head += completed;
    currently_falling_count -= completed;

}


/*
 * Check for any disconnected bubbles
 */
void floating_bubble_check (void)
{
    /* TODO: Rather than clearing the float-map just before use, it could
     *       be marked dirty, to be automatically zeroed when some spare
     *       time is available. Eg, during the VDP active-area period. */
    memset (float_map, FLOAT_UNCHECKED, sizeof (float_map));

    uint8_t stack [80];
    uint8_t stack_size = 0;
    uint8_t match_count = 0;

    /* The stack is initialized with the top row of bubbles */
    for (uint8_t i = 0; i < 8; i++)
    {
        if (game_board [10 + i] != BUBBLE_NONE)
        {
            stack [stack_size++] = 10 + i;
            float_map [10 + i] = FLOAT_QUEUED;
        }
    }

    while (stack_size > 0)
    {
        uint8_t match_pos = stack [--stack_size];

        /* Skip matching bubbles that have already been explored. */
        if (float_map [match_pos] == FLOAT_CONNECTED)
        {
            continue;
        }

        /* Add each newly-found connected neighbour to the stack */
        for (uint8_t n = 0; n < 6; n++)
        {
            uint8_t neighbour = match_pos + neighbours [n];

            if (float_map [neighbour] == FLOAT_UNCHECKED &&
                game_board [neighbour] != BUBBLE_NONE)
            {
                stack [stack_size++] = neighbour;
                float_map [neighbour] = FLOAT_QUEUED;
            }
        }

        /* Mark this bubble as having been explored. */
        float_map [match_pos] = FLOAT_CONNECTED;
        match_count++;
    }

    /* Clear each of the unconnected bubbles */
    for (uint8_t i = 102; i >= 10; i--)
    {
        if (game_board [i] != BUBBLE_NONE && float_map [i] != FLOAT_CONNECTED)
        {
            set_bubble (i, BUBBLE_NONE);
            fall_queue [fall_queue_tail++ & 0x3f] = i;
        }
    }
}


/* We have to spend the VRAM either way, so draw into the background
 * instead of using sprites. This way we're at least not eating into
 * the limiting number of sprites per line.
 */
void draw_next_bubble (void)
{
    uint32_t composite_left [4];
    uint32_t composite_right [4];

    uint32_t bubble_bottom_left [3];
    uint32_t bubble_bottom_right [3];

    SMS_mapROMBank (3); /* Bubbles */

    uint16_t bubble_index = next_bubble * BUBBLE_MAX;

    /* Top 12 px (bubble alone) */
    uint16_t dest = NEXT_BUBBLE_PATTERN << 5;
    SMS_VRAMmemcpy (dest,      &bubbles_patterns [bubbles_panels [bubble_index] [0] << 3], 32);
    SMS_VRAMmemcpy (dest + 32, &bubbles_patterns [bubbles_panels [bubble_index] [2] << 3], 16);
    SMS_VRAMmemcpy (dest + 64, &bubbles_patterns [bubbles_panels [bubble_index] [1] << 3], 32);
    SMS_VRAMmemcpy (dest + 96, &bubbles_patterns [bubbles_panels [bubble_index] [3] << 3], 16);

    memcpy (bubble_bottom_left,  &bubbles_patterns [bubbles_panels [bubble_index] [2] << 3] + 4, 12);
    memcpy (bubble_bottom_right, &bubbles_patterns [bubbles_panels [bubble_index] [3] << 3] + 4, 12);

    SMS_mapROMBank (2);

    /* Bottom 4 px (bubble and grass) */
    memcpy (composite_left,  &grass_patterns [4],  16);
    memcpy (composite_right, &grass_patterns [12], 16);

    /* A bitmask is used to keep the grass blades in front of the bubble */

    /* Overlap line 1 */
    composite_left  [0] |= (bubble_bottom_left  [0] & 0xaeaeaeae);
    composite_right [0] |= (bubble_bottom_right [0] & 0xaaaaaaaa);

    /* Overlap line 2 */
    composite_left  [1] |= (bubble_bottom_left  [1] & 0x2a2a2a2a);
    composite_right [1] |= (bubble_bottom_right [1] & 0xaaaaaaaa);

    /* Overlap line 3 */
    composite_left  [2] |= (bubble_bottom_left  [2] & 0x22222222);
    composite_right [2] |= (bubble_bottom_right [2] & 0x88888888);

    SMS_VRAMmemcpy (dest +  48, composite_left, 16);
    SMS_VRAMmemcpy (dest + 112, composite_right, 16);
}


/*
 * Prepare a bubble for the 'next' slot.
 */
void prepare_next_bubble (void)
{
    next_bubble = BUBBLE_NONE;

    /* TODO: Find a solution with a more constant time */
    while (next_bubble == BUBBLE_NONE)
    {
        bubble_t try = BUBBLE_CYAN + (rand () & 0x07);
        if (colour_count [try])
        {
            next_bubble = try;
        }
    }

    draw_next_bubble ();
}


/*
 * Load the next bubble into the launcher.
 */
void load_next_bubble (void)
{
    active_bubble_colour = next_bubble;

    /* Reset the coordinates for the next bubble */
    active_bubble_x = LAUNCH_FROM_X;
    active_bubble_y = LAUNCH_FROM_Y;

    /* Prepare the next new bubble */
    prepare_next_bubble ();

    state = BUBBLE_READY;
}


/*
 * Load a level into the game-board.
 */
void load_level (uint8_t level)
{
    uint8_t *level_board = level_data [level - 1];
    bubble_t bubble;

    /* TODO: Consider, make the level-data format and the game-board
     *       format match, then memcpy can be used instead of a switch */

    for (uint32_t i = 10; i < 114; i++)
    {
        switch (level_board [i])
        {
            case 'R':   bubble = BUBBLE_RED;      break;
            case 'G':   bubble = BUBBLE_GREEN;    break;
            case 'B':   bubble = BUBBLE_CYAN;     break;
            case 'Y':   bubble = BUBBLE_YELLOW;   break;
            case 'P':   bubble = BUBBLE_PURPLE;   break;
            case 'O':   bubble = BUBBLE_ORANGE;   break;
            case 'D':   bubble = BUBBLE_BLACK;    break;
            case 'L':   bubble = BUBBLE_WHITE;    break;
            case 'C':   bubble = BUBBLE_CLEAR;    break;
            default:    bubble = BUBBLE_NONE;
        }

        set_bubble (i, bubble);
        draw_bubble (i, bubble);
    }
}


/* Draw the UI elements that get copied into the game
 * board vram area (yellow line and 'next' signpost.
 *
 * Note on coordinates:
 *  -> 640 bytes per column.
 *  -> 32 bytes per tile within column.
 *  -> 4 bytes per line within column.
 */
void draw_game_board_ui (void)
{
    /* Yellow line */
    const uint32_t yellow_line [2] = { 0x0000ffff, 0x00ff0000 };
    uint16_t line_index = 18 * 32 + 4;
    for (uint8_t x = 0; x < 16; x++)
    {
        SMS_VRAMmemcpy (line_index, yellow_line, 8);
        line_index += 640;
    }

    /* Next bubble signpost, 9px high. Note that the
     * top line is re-used for the bottom line. */
    uint16_t dest = 12 * 640 + 150 * 4;
    for (uint8_t x = 0; x < 32; x += 8)
    {
        SMS_VRAMmemcpy (dest, &next_patterns [x], 32);
        SMS_VRAMmemcpy (dest + 32, &next_patterns [x], 4);
        dest += 640;
    }
}


/*
 * Play one round of the game.
 */
bool play_level (uint8_t level)
{
    /* Update round number */
    text_draw_round (level);

    /* Reset the timer & launcher */
    text_draw_time ();
    time_minutes = 0;
    time_seconds = 0;
    time_frames = 0;
    launcher_aim = LAUNCHER_AIM_CENTRE;


    /* TODO: Hide the clearing of the board / loading of level.
     *       Eg, use an all-blue sprite palette or do something
     *       with the name-table. */

    /* Populate game-board with level */
    load_level (level);

    draw_game_board_ui (); /* UI is drawn after load_level, to avoid drawing over line-crossing bubbles. */
    prepare_next_bubble (); /* Don't let the 'next' bubble carry over from the previous level */
    load_next_bubble ();

    while (true)
    {
        SMS_waitForVBlank ();
        uint16_t key_pressed = SMS_getKeysPressed ();
        uint16_t key_status = SMS_getKeysStatus ();

        /* Aiming */
        uint16_t key_horizontal = key_status & (PORT_A_KEY_LEFT | PORT_A_KEY_RIGHT);
        uint16_t key_vertical = key_status & (PORT_A_KEY_UP | PORT_A_KEY_DOWN);
        if (key_horizontal)
        {
            if (key_horizontal == PORT_A_KEY_LEFT && launcher_aim > LAUNCHER_AIM_MIN)
            {
                launcher_aim -= 1;
                if (tick_holdoff == 0)
                {
                    PSGSFXPlay (tick_sound, 0x0f);
                    tick_holdoff = 4;
                }
            }
            else if (key_horizontal == PORT_A_KEY_RIGHT && launcher_aim < LAUNCHER_AIM_MAX)
            {
                launcher_aim += 1;
                if (tick_holdoff == 0)
                {
                    PSGSFXPlay (tick_sound, 0x0f);
                    tick_holdoff = 4;
                }
            }
        }
        else if (key_vertical)
        {
            if (key_vertical == PORT_A_KEY_UP)
            {
                if (launcher_aim < LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim++;
                    if (tick_holdoff == 0)
                    {
                        PSGSFXPlay (tick_sound, 0x0f);
                        tick_holdoff = 4;
                    }
                }
                else if (launcher_aim > LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim--;
                    if (tick_holdoff == 0)
                    {
                        PSGSFXPlay (tick_sound, 0x0f);
                        tick_holdoff = 4;
                    }
                }
            }
            else if (key_vertical == PORT_A_KEY_DOWN)
            {
                if (launcher_aim > LAUNCHER_AIM_MIN && launcher_aim < LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim--;
                }
                else if (launcher_aim > LAUNCHER_AIM_CENTRE && launcher_aim < LAUNCHER_AIM_MAX)
                {
                    launcher_aim++;
                    PSGSFXPlay (pop_sound, 0x0f);
                }
            }
        }

        if (key_pressed & PORT_A_KEY_1)
        {
            if (state == BUBBLE_READY)
            {
                active_bubble_velocity_x = angle_data [launcher_aim].x;
                active_bubble_velocity_y = angle_data [launcher_aim].y;
                state = BUBBLE_MOVING;
                PSGSFXPlay (launch_sound, 0x0f);
            }
            else if (state == ROUND_IS_LOST && grey_wash_step == 0xff)
            {
                return false;
            }
            else if (state == ROUND_IS_WON && currently_falling_count == 0)
            {
                return true;
            }
        }

        else if (key_pressed & PORT_A_KEY_2)
        {
            if (state == ROUND_IS_LOST && grey_wash_step == 0xff)
            {
                return false;
            }
            else if (state == ROUND_IS_WON && currently_falling_count == 0)
            {
                return true;
            }
        }

        /* Movement */
        if (state == BUBBLE_MOVING)
        {
            /* Placing the collision-check here rather than after the velocity
             * is applied, allows the bubble to enjoy a frame of visible collision.
             * This seems to be what happens on the PS1 version.
             *
             * Revisit this decision once a couple of extra frames are added to
             * smooth the transition from the overlap-frame to the final position.
             * If it still looks kinda choppy, then maybe remove the frame of
             * overlap. */
            if (active_bubble_collision ())
            {
                active_bubble_calculate_board_position ();
                state = BUBBLE_LANDED;

                if (active_bubble_try_pop ())
                {
                    PSGSFXPlay (pop_sound, 0x0f);

                    /* The bubble has popped, check if this triggers any others to fall */
                    floating_bubble_check ();

                    /* Check if the level has been completed */
                    bool bubbles_remaining = false;
                    for (uint8_t i = 10; i < 18; i++)
                    {
                        if (game_board [i] != BUBBLE_NONE)
                        {
                            bubbles_remaining = true;
                            break;
                        }
                    }
                    if (!bubbles_remaining)
                    {
                        state = ROUND_IS_WON;
                    }
                }
                else
                {
                    set_bubble (active_bubble_board_position, active_bubble_colour);
                    draw_bubble (active_bubble_board_position, active_bubble_colour);
                    if (active_bubble_board_position >= 105)
                    {
                        /* A bubble crossed the line, end the round. */
                        grey_wash_step = GREY_WASH_BEGIN;
                        state = ROUND_IS_LOST;
                    }
                }

                if (state == BUBBLE_LANDED)
                {
                    load_next_bubble ();
                }
            }
            else
            {
                active_bubble_x += active_bubble_velocity_x;
                active_bubble_y += active_bubble_velocity_y;

                /* Bounce off the walls.
                 * Note, maths is simplified and uses modulo 16-bit*/
                /* TODO: Tune the subpixel value to give the half-pixel at the edge like the PS1 version has */
                if (active_bubble_x < LEFT_EDGE)
                {
                    active_bubble_x = 0x5000 - active_bubble_x;
                    active_bubble_velocity_x = -active_bubble_velocity_x;
                    PSGSFXPlay (bounce_sound, 0x0f);
                }
                else if (active_bubble_x > RIGHT_EDGE)
                {
                    active_bubble_x = 0x3000 - active_bubble_x;
                    active_bubble_velocity_x = -active_bubble_velocity_x;
                    PSGSFXPlay (bounce_sound, 0x0f);
                }
            }
        }

        /* Sprites */
        SMS_initSprites ();
        if (state == BUBBLE_READY || state == BUBBLE_MOVING)
        {
            draw_active_bubble ();
            text_update_time ();
        }
        else if (state == ROUND_IS_LOST)
        {
            wash_bubbles_grey ();
        }
        draw_pip ();
        draw_fallers ();
        SMS_copySpritestoSAT ();

        if (tick_holdoff > 0)
        {
            tick_holdoff--;
        }
    }
}


/*
 * Frame Interrupt
 */
static void frame_interrupt (void)
{
    PSGFrame ();
    PSGSFXFrame ();
}


/*
 * Entry point.
 */
void main (void)
{
    /* Setup */
    SMS_setBackdropColor (0);
    SMS_loadBGPalette (background_palette);
    SMS_loadSpritePalette (sprite_palette);
    SMS_useFirstHalfTilesforSprites (false); /* The first half of vram holds the game board */

    PSGSetSFXVolumeAttenuation (0);
    SMS_setFrameInterruptHandler (frame_interrupt);

    /* Patterns 0-319: Game board */
    for (uint16_t i = 0; i < 320; i++)
    {
        UNSAFE_SMS_load1Tile (blue_tile, i);
    }

    /* Bubbles as sprites */
    SMS_mapROMBank (3); /* Bubbles */
    for (uint8_t i = 0; i < 9; i++)
    {
        const bubble_t bubble_type [9] = { BUBBLE_CYAN, BUBBLE_RED, BUBBLE_GREEN, BUBBLE_YELLOW,
                                           BUBBLE_PURPLE, BUBBLE_ORANGE, BUBBLE_BLACK, BUBBLE_WHITE, BUBBLE_CLEAR };
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [0] << 3], BUBBLE_PATTERN + (i << 2)    , 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [1] << 3], BUBBLE_PATTERN + (i << 2) + 1, 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [2] << 3], BUBBLE_PATTERN + (i << 2) + 2, 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [3] << 3], BUBBLE_PATTERN + (i << 2) + 3, 32);
    }
    SMS_mapROMBank (2);

    /* Indicator pip */
    SMS_loadTiles (pip_patterns, PIP_PATTERN, sizeof (pip_patterns));

    /* Plain blue pattern */
    SMS_loadTiles (blue_tile, BLUE_TILE_PATTERN, sizeof (blue_tile));

    /* Grass patterns */
    SMS_loadTiles (grass_patterns, GRASS_PATTERN, sizeof (grass_patterns));

    /* Border patterns */
    SMS_loadTiles (border_patterns, BORDER_PATTERN, sizeof (border_patterns));

    /* Tile-map: Background and border around game board */
    for (uint8_t y = 0; y < 24; y++)
    {
        uint16_t row [32];

        /* Blue fill */
        for (uint8_t x = 0; x < 32; x++)
        {
            row [x] = BLUE_TILE_PATTERN;
        }

        /* Wood-texture border */
        if (y == 0) /* First line */
        {
            row [4] = BORDER_PATTERN + 0;
            for (uint8_t x = 5; x < 21; x++)
            {
                row [x] = BORDER_PATTERN + 1;
            }
            row [21] = BORDER_PATTERN + 2;
        }
        else if (y < 22) /* Middle */
        {
            row [4] = BORDER_PATTERN + 3;
            row [21] = BORDER_PATTERN + 5;
            for (uint8_t x = 5; x < 21; x++)
            {
                /* Trying a lighter blue in the game-board, using the sprite
                 * palette. Doesn't reach all the way to the grass though..
                 *
                 * Either the background palette needs to add the light blue,
                 * or the sprite palette needs to have the grass, which would
                 * lock in the green bubble. */
                row [x] = BLUE_TILE_PATTERN | 0x0800;
            }
            /* Next bubble area */
            if (y == 21)
            {
                row [18] = NEXT_BUBBLE_PATTERN + 0 | 0x0800;
                row [19] = NEXT_BUBBLE_PATTERN + 2 | 0x0800;
            }
        }
        else if (y == 22) /* Grass row 1 */
        {
            for (uint8_t x = 0; x < 32; x++)
            {
                row [x] = GRASS_PATTERN + (x & 0x01);
            }
            /* While trying out a lighter blue within the game-board area,
             * special light-blue-sky grass tiles are needed to match. */
            for (uint8_t x = 5; x < 21; x++)
            {
                row [x] += 4;
            }
            row [4] = BORDER_PATTERN + 6;
            row [18] = NEXT_BUBBLE_PATTERN + 1 | 0x0800;
            row [19] = NEXT_BUBBLE_PATTERN + 3 | 0x0800;
            row [21] = BORDER_PATTERN + 7;
        }
        else if (y == 23) /* Grass row 2 */
        {
            for (uint8_t x = 0; x < 32; x++)
            {
                row [x] = GRASS_PATTERN + 2 + (x & 0x01);
            }
        }

        SMS_loadTileMapArea (0, y, row, 32, 1);
    }

    /* Text */
    text_load_patterns ();
    text_draw_time ();
    text_draw_best ();

    /* Tile-map: Game board strips
     *
     * The game board is organized into 16 vertical strips, each tile in these
     * strips which has a unique pattern associated with it. The bubbles can
     * be freely drawn into these strips without needing to worry about tile
     * boundaries. */
    uint16_t pattern_index = 0;
    for (uint8_t strip = 0; strip < 16; strip++)
    {
        uint16_t strip_map [20];

        for (uint8_t y = 0; y < 20; y++)
        {
            /* Use the sprite palette */
            strip_map [y] = 0x0800 | pattern_index;


            pattern_index += 1;
        }
        SMS_loadTileMapArea (5 + strip, 1, strip_map, 1, 20);
    }

    memset (game_board, BUBBLE_NONE, sizeof (game_board));
    for (uint16_t i = 0; i < 320; i++)
    {
        SMS_loadTiles (blue_tile, i, 32);
    }

    SMS_displayOn ();

    uint8_t level = 1;
    while (true)
    {
        if (play_level (level) == true)
        {
            level += 1;
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
