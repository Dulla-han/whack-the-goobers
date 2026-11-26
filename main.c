/**
 * @file main.c
 * @brief Main control loop for the "Three Little Guys + POW overlay" game on MZ_APO.
 *
 * This file contains the program entry point and the primary game loop logic for the
 * "Three Little Guys + POW overlay" project. It manages hardware initialization, user
 * input, display updates, and the transitions between the main gameplay, title,
 * credits, and end screens. 
 *
 * The main function sets up the framebuffer, maps memory regions, and manages state
 * transitions and score logic according to button inputs. Screen rendering is
 * performed via the functions defined in @ref display.h and @ref screens.h.
 *
 * @authors Tobias Lemeer, Jakub Zimmer
 * @date 2025-05-29
 * @license MIT
 *
 * @note CTU FEE student project, 2025.
 */

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"
#include "font_types.h" /**< Font data, provides font_descriptor_t */
#include "display.h"
#include "screens.h"

extern font_descriptor_t font_rom8x16;

/// Macro to create 24-bit RGB color in the format expected by the LEDs.
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (b))

#define OFFSET 10

/**
 * @brief Program entry point and main game loop.
 *
 * Initializes hardware (LCD, framebuffer, SPILED), and manages the state machine for
 * title screen, main game loop, and end screen. Handles user input from the three
 * push-buttons and updates the LED colors and display accordingly.
 *
 * @return Returns 0 on normal exit, nonzero on hardware initialization error.
 */
int main(void)
{
    /* ---------- map physical addresses -------------------------------- */
    unsigned char *parlcd = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (!parlcd)
        return 1;

    unsigned char *spiled = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0);
    if (!spiled)
        return 1;

    volatile uint32_t *led1 = (uint32_t *)(spiled + SPILED_REG_LED_RGB1_o);
    volatile uint32_t *led2 = (uint32_t *)(spiled + SPILED_REG_LED_RGB2_o);

    parlcd_hx8357_init(parlcd);

    fb = calloc(LCD_W * LCD_H, sizeof(unsigned short));
    if (!fb)
        return 1;
    /* ---------- pre‑compute constant positions ------------------------ */
    const int guy_w = IMAGE_WIDTH;
    const int guy_h = IMAGE_HEIGHT;

    /* bottom row of sprites */
    const int y_guys = LCD_H - guy_h;
    const int x_left = 0;
    const int x_mid = (LCD_W - guy_w) / 2;
    const int x_right = LCD_W - guy_w;
    int time = 0;
    int score = 0;
    const int score_x = LCD_W / 2;
    const int score_y = LCD_H / 2 - 30;

    bool can_be_hit_left = false;
    bool can_be_hit_middle = false;
    bool can_be_hit_right = false;

    bool titleScreenShow = true;
    bool playingGame = false;
    bool finishedGame = false;
    bool quit = false;

    int random_count_left = rand() % 4 + 1;
    int random_count_middle = rand() % 4 + 1;
    int random_count_right = rand() % 4 + 1;

    int count_since_guy_left = 0;
    int count_since_guy_middle = 0;
    int count_since_guy_right = 0;

    const int timer_x = LCD_W - (2 * FONT_WIDTH * PRINT_SCALE + 1) - 2;
    const int timer_y = 2;

    const int y_pow = y_guys - OFFSET; /* may be negative – draw_pixel checks */

    struct timespec frame_delay = {.tv_sec = 0, .tv_nsec = 150 * 1000 * 1000};
    /* ====================== main render loop ========================== */
    const int TOTAL_SEC = 30;

    /* remember the start time */
    struct timespec start_ts;
    int remaining;

    while (!quit)
    {
        if (titleScreenShow)
        {
            draw_title_screen(spiled, parlcd, &titleScreenShow, &playingGame, &quit, &start_ts);
        } else if (finishedGame)
        {
             draw_finished_game(spiled, parlcd, &remaining, score_x, score_y, &score, &quit, &playingGame, &finishedGame, &start_ts);
             count_since_guy_middle = 0;
             count_since_guy_left = 0;
             count_since_guy_right = 0;
             random_count_left = rand() % 4 + 1;
             random_count_middle = rand() % 4 + 1;
             random_count_right = rand() % 4 + 1;
             can_be_hit_left = false;
             can_be_hit_middle = false;
             can_be_hit_right = false;

        } else if (playingGame)
        {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);

            int elapsed = (int)(now.tv_sec - start_ts.tv_sec);
            remaining = TOTAL_SEC - elapsed;
            time = TOTAL_SEC - remaining;

            uint32_t knobs = *(volatile uint32_t *)(spiled + SPILED_REG_KNOBS_8BIT_o);

            /* -------- clear framebuffer ------------------------------------ */
            for (size_t i = 0; i < LCD_W * LCD_H; ++i)
                fb[i] = 0x0000;

            /* -------- draw bottom sprites ---------------------------------- */
            if (!can_be_hit_right)
            {
                draw_hole(x_right, y_guys);
            }
            else
            {
                draw_blue_guy(x_right, y_guys);
            }

            if (!can_be_hit_middle)
            {
                draw_hole(x_mid, y_guys);
            }
            else
            {
                draw_green_guy(x_mid, y_guys);
            }

            if (!can_be_hit_left)
            {
                draw_hole(x_left, y_guys);
            }
            else
            {
                draw_red_guy(x_left, y_guys);
            }

            /* -------- overlay "POW" while button is held ------------------- */
            uint32_t buttons = knobs & (BTN_LEFT | BTN_MID | BTN_RIGHT);

            uint32_t colour;
            if (buttons & BTN_LEFT)
                colour = RGB(0xFF, 0x00, 0x00); /* red   */
            else if (buttons & BTN_MID)
                colour = RGB(0x00, 0xFF, 0x00); /* green */
            else if (buttons & BTN_RIGHT)
                colour = RGB(0x00, 0x00, 0xFF); /* blue  */
            else
                colour = RGB(0x00, 0x00, 0x00); /* off   */

            *led1 = colour;
            *led2 = colour;

            if ((buttons & BTN_LEFT) && can_be_hit_left)
            {
                draw_pow(x_left, y_pow);
                can_be_hit_left = false;
                score += STARTING_SCORE - count_since_guy_left * 101;
                random_count_left = time + (rand() % 4 + 1);
            } else if ((buttons & BTN_LEFT) && !can_be_hit_left) {
                score -= 1000;
                if (score < 0) score = 0;
            }
            if ((buttons & BTN_MID) && can_be_hit_middle)
            {
                draw_pow(x_mid, y_pow);
                can_be_hit_middle = false;
                score += STARTING_SCORE - count_since_guy_middle * 101;
                random_count_middle = time + (rand() % 4 + 1);
            } else if ((buttons & BTN_MID) && !can_be_hit_middle) {
                score -= 1000;
                if (score < 0) score = 0;
            }
            if ((buttons & BTN_RIGHT) && can_be_hit_right)
            {
                draw_pow(x_right, y_pow);
                can_be_hit_right = false;
                score += STARTING_SCORE - count_since_guy_right * 101;
                random_count_right = time + (rand() % 4 + 1);
            } else if ((buttons & BTN_RIGHT) && !can_be_hit_right) {
                score -= 1000;
                if (score < 0) score = 0;
            }

            if ((random_count_left <= time) && (can_be_hit_left == false))
            {
                can_be_hit_left = true;
                count_since_guy_left = 0;
                random_count_left = -1;
            }
            if ((random_count_middle <= time) && (can_be_hit_middle == false))
            {
                can_be_hit_middle = true;
                count_since_guy_middle = 0;
                random_count_middle = -1;
            }
            if ((random_count_right <= time) && (can_be_hit_right == false))
            {
                can_be_hit_right = true;
                count_since_guy_right = 0;
                random_count_right = -1;
            }

            /* -------- draw countdown timer --------------------------------- */
            if (remaining < 11)
                draw_timer(timer_x, timer_y, remaining, 0xF000);
            else
                draw_timer(timer_x, timer_y, remaining, 0xFFFE); /* white digits */

            /* -------- draw score --------- --------------------------------- */
            draw_score(score_x, score_y, score, 0xFFFE);

            /* -------- flush frame to LCD ----------------------------------- */
            parlcd_write_cmd(parlcd, 0x2c);
            for (size_t i = 0; i < LCD_W * LCD_H; ++i)
                parlcd_write_data(parlcd, fb[i]);

            clock_nanosleep(CLOCK_MONOTONIC, 0, &frame_delay, NULL);
            time = TOTAL_SEC - remaining;

            count_since_guy_left++;
            count_since_guy_middle++;
            count_since_guy_right++;

            if (remaining <= 0)
            { // FIX: use braces!
                playingGame = false;
                finishedGame = true;
            }
        }
        // No need for an else if (quit) here
    }

    /* -------- blank display on exit ---------------------------------- */
    parlcd_write_cmd(parlcd, 0x2c);
    for (size_t i = 0; i < LCD_W * LCD_H; ++i)
        parlcd_write_data(parlcd, 0);

    printf("Good‑bye!, Times up!\n");
    free(fb);
    return 0;
}
