/*
 * Bubbles for Master System
 * A Bust-A-Move clone for the Sega Master System
 *
 * TODOs:
 *  - Seed rand from R, generate random bubbles.
 *  - Save & restore rand seed.
 *  - Draw the launcher arrow
 *  - High-score table
 *  - Colourblind mode
 *  - Shaking & dropping
 *  - Music
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "SMSlib.h"

#include "vram.h"
#include "data.h"
#include "text.h"

#define TARGET_SMS
#include "../game_tile_data/patterns.h"
#include "../game_tile_data/pattern_index.h"
#include "../game_tile_data/palette.h"

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
    BUBBLE_MAX
} bubble_t;

uint8_t launcher_aim = LAUNCHER_AIM_CENTRE;

/* The "active bubble" is the single bubble that is currently:
 *  - Loaded into the bubble-launcher, or
 *  - In flight, or
 *  - In the process of landing.
 * A lot of functionality interacts with the active bubble, so
 * the state is made global for quick access. */
static bubble_t active_bubble_colour = BUBBLE_CYAN;
static uint16_t active_bubble_velocity_x = 0;
static uint16_t active_bubble_velocity_y = 0;
static uint16_t active_bubble_x = LAUNCH_FROM_X;
static uint16_t active_bubble_y = LAUNCH_FROM_Y;
static uint8_t active_bubble_board_position = 0;

typedef enum game_state_e {
    BUBBLE_READY = 0,
    BUBBLE_MOVING
} game_state_t;

game_state_t state = BUBBLE_READY;

uint8_t time_minutes = 0;
uint8_t time_seconds = 0;
uint8_t time_frames = 0;

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
 */
bubble_t game_board [143];
#define NEIGH_TOP_LEFT    -10
#define NEIGH_TOP_RIGHT    -9
#define NEIGH_LEFT         -1
#define NEIGH_RIGHT         1
#define NEIGH_BOTTOM_LEFT   9
#define NEIGH_BOTTOM_RIGHT 10
static int8_t neighbours [6] = { NEIGH_TOP_LEFT, NEIGH_TOP_RIGHT,   NEIGH_LEFT,
                                 NEIGH_RIGHT,    NEIGH_BOTTOM_LEFT, NEIGH_BOTTOM_RIGHT };

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
void set_bubble (uint8_t position, bubble_t bubble)
{
    game_board [position] = bubble;

    /* Neighbours */
    bubble_t neigh_tl = game_board [position + NEIGH_TOP_LEFT];
    bubble_t neigh_tr = game_board [position + NEIGH_TOP_RIGHT];
    bubble_t neigh_bl = game_board [position + NEIGH_BOTTOM_LEFT];
    bubble_t neigh_br = game_board [position + NEIGH_BOTTOM_RIGHT];

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

    /* Left strip */
    SMS_VRAMmemcpy (left_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tl] [0] << 3], 32);
    SMS_VRAMmemcpy (left_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_bl] [2] << 3], 32);

    /* Right strip */
    SMS_VRAMmemcpy (right_strip,      &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_tr] [1] << 3], 32);
    SMS_VRAMmemcpy (right_strip + 32, &bubbles_patterns [bubbles_panels [bubble * BUBBLE_MAX + neigh_br] [3] << 3], 32);
}


/*
 * Convert a half-bubble to grey.
 */
void set_halfbubble_grey (uint8_t position, bool top_half)
{
    bubble_t bubble = game_board [position];

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
 * TODO: Find out if the launcher arrow remain responsive during this.
 */
void wash_bubbles_grey (void)
{
    /* TODO: Consider moving this to data.h and using elsewhere to simplify maths. */
    uint8_t row_start [11] = { 10, 20, 29, 39, 48, 58, 67, 77, 86, 96, 105 };

    for (uint8_t half_row = 21; half_row != 0xff; half_row--)
    {
        uint8_t row = half_row >> 1;
        bool top_half = ~half_row & 0x01;

        uint8_t row_length = (row & 0x01) ? 7 : 8;

        /* For now this function is blocking until the sweep ends.
         * Update one line per every couple of frames. */
        SMS_waitForVBlank ();
        SMS_waitForVBlank ();
        for (uint8_t i = 0; i < row_length; i++)
        {
            set_halfbubble_grey (row_start [row] + i, top_half);
        }
    }
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
        bubble_t type = game_board [position];
        falling_bubble_t *new = &currently_falling [currently_falling_tail++ & 0x03];

        new->pattern = BUBBLE_PATTERN + ((type - 1) << 2);
        new->x = game_board_x [position];
        new->y = game_board_y [position];
        new->velocity = 1;
        new->frame = 0;

        set_bubble (position, BUBBLE_NONE);
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
            fall_queue [fall_queue_tail++ & 0x3f] = i;
        }
    }
}


/*
 * Load the next bubble into the launcher.
 */
void load_next_bubble (void)
{
    /* Reset the coordinates for the next bubble */
    active_bubble_x = LAUNCH_FROM_X;
    active_bubble_y = LAUNCH_FROM_Y;

    /* For now, just cycle between the colours */
    active_bubble_colour += 1;
    if (active_bubble_colour >= BUBBLE_MAX)
    {
        active_bubble_colour = BUBBLE_CYAN;
    }

    state = BUBBLE_READY;
}


/*
 * Play one round of the game.
 * For now, the only 'level' is an all-blank starting position.
 */
void play_level (void)
{
    /* Clear the game board */
    memset (game_board, BUBBLE_NONE, sizeof (game_board));

    /* Reset the timer */
    text_draw_time ();
    time_minutes = 0;
    time_seconds = 0;
    time_frames = 0;

    /* TODO: Hide the clearing of the board. Eg, use an all-blue sprite
     *       palette or update the name-table to show all-blank tiles. */
    for (uint16_t i = 0; i < 320; i++)
    {
        SMS_loadTiles (blue_tile, i, 32);
    }

    /* Yellow line */
    const uint32_t yellow_line [2] = { 0x000000ff, 0x0000ff00 };
    uint16_t line_index = 18 * 32 + 4;
    for (uint8_t x = 0; x < 16; x++)
    {
        SMS_VRAMmemcpy (line_index, yellow_line, 8);
        line_index += 640;
    }

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
            }
            else if (key_horizontal == PORT_A_KEY_RIGHT && launcher_aim < LAUNCHER_AIM_MAX)
            {
                launcher_aim += 1;
            }
        }
        else if (key_vertical)
        {
            if (key_vertical == PORT_A_KEY_UP)
            {
                if (launcher_aim < LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim++;
                }
                else if (launcher_aim > LAUNCHER_AIM_CENTRE)
                {
                    launcher_aim--;
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

                if (active_bubble_try_pop ())
                {
                    /* The bubble has popped, check if this triggers any others to fall */
                    floating_bubble_check ();
                }
                else
                {
                    set_bubble (active_bubble_board_position, active_bubble_colour);
                    if (active_bubble_board_position >= 105)
                    {
                        /* A bubble crossed the line, end the round. */
                        /* TODO: Check with the PS1 game, what's left in the launcher?
                         * Nothing, a coloured bubble, or a grey bubble? */
                        wash_bubbles_grey ();

                        /* Wait for a button press then exit this attempt at the round. */
                        while (true)
                        {
                            key_pressed = SMS_getKeysPressed ();
                            if (key_pressed & (PORT_A_KEY_1 | PORT_A_KEY_2))
                            {
                                return;
                            }
                        }
                    }
                }

                load_next_bubble ();
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
                }
                else if (active_bubble_x > RIGHT_EDGE)
                {
                    active_bubble_x = 0x3000 - active_bubble_x;
                    active_bubble_velocity_x = -active_bubble_velocity_x;
                }
            }
        }

        /* Sprites */
        SMS_initSprites ();
        draw_active_bubble ();
        draw_pip ();
        draw_fallers ();
        text_update_time ();
        SMS_copySpritestoSAT ();
    }
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

    /* Patterns 0-319: Game board */
    for (uint16_t i = 0; i < 320; i++)
    {
        UNSAFE_SMS_load1Tile (blue_tile, i);
    }

    /* Bubbles as sprites */
    for (uint8_t i = 0; i < 4; i++)
    {
        const bubble_t bubble_type [4] = { BUBBLE_CYAN, BUBBLE_RED, BUBBLE_GREEN, BUBBLE_YELLOW };
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [0] << 3], BUBBLE_PATTERN + (i << 2)    , 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [1] << 3], BUBBLE_PATTERN + (i << 2) + 1, 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [2] << 3], BUBBLE_PATTERN + (i << 2) + 2, 32);
        SMS_loadTiles (&bubbles_patterns [bubbles_panels [bubble_type [i] * BUBBLE_MAX] [3] << 3], BUBBLE_PATTERN + (i << 2) + 3, 32);
    }

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
    text_draw_round (1);
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

    SMS_displayOn ();

    while (true)
    {
        play_level ();
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
