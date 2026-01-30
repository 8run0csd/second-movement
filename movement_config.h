/*
 * MIT License
 *
 * Copyright (c) 2022 Joey Castillo
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MOVEMENT_CONFIG_H_
#define MOVEMENT_CONFIG_H_

#include "movement_faces.h"

/* The F() below is a trick to be able to get the face as well as its name */

#define PRIMARY_FACES(F) \
    F(stock_clock) \
    F(fast_stopwatch) \
    F(countdown) \
    F(timer) \
    F(alarm) \
    F(moon_phase) \
    F(sunrise_sunset) 

#define SECONDARY_FACES(F) \
    F(close_enough) \
    F(hexa_decimal) \
    F(binary) \
    F(ke_decimal_time) \
    F(sleep_calculator) \
    F(step_counter) \
    F(tally) \
    F(deadline) \
    F(pulsometer) \
    F(tcg_life_counter) \
    F(tide) \
    F(character_set)

#define TERTIARY_FACES(F) \
    F(voltage) \
    F(temperature_logging) \
    F(activity_logging) \
    F(accelerometer_status) \
    F(endless_runner) \
    F(metronome) \
    F(ping) \
    F(higher_lower_game) \
    F(lander) \
    F(simon) \
    F(probability) \
    F(wordle) \
    F(tunes) \
    F(page_ordering) \
    F(settings) \
    F(set_time) \
    F(finetune) \
    F(nanosec) \
    F(rtccount) 


/* Determines the intensity of the led colors
 * Set a hex value 0-15 with 0x0 being off and 0xF being max intensity
 */
#define MOVEMENT_DEFAULT_RED_COLOR 0xF
#define MOVEMENT_DEFAULT_GREEN_COLOR 0x0
#define MOVEMENT_DEFAULT_BLUE_COLOR 0x0

/* Set to true for 24h mode or false for 12h mode */
#define MOVEMENT_DEFAULT_24H_MODE true

/* Enable or disable the sound on mode button press */
#define MOVEMENT_DEFAULT_BUTTON_SOUND true

#define MOVEMENT_DEFAULT_BUTTON_VOLUME WATCH_BUZZER_VOLUME_SOFT
#define MOVEMENT_DEFAULT_SIGNAL_VOLUME WATCH_BUZZER_VOLUME_LOUD
#define MOVEMENT_DEFAULT_ALARM_VOLUME WATCH_BUZZER_VOLUME_LOUD

/* Set the timeout before switching back to the main watch face
 * Valid values are:
 * 0: 60 seconds
 * 1: 2 minutes
 * 2: 5 minutes
 * 3: 30 minutes
 */
#define MOVEMENT_DEFAULT_TIMEOUT_INTERVAL 1

/* Set the timeout before switching to low energy mode
 * Valid values are:
 * 0: Never
 * 1: 10 minutes
 * 2: 1 hour
 * 3: 2 hours
 * 4: 6 hours
 * 5: 12 hours
 * 6: 1 day
 * 7: 7 days
 */
#define MOVEMENT_DEFAULT_LOW_ENERGY_INTERVAL 1

/* Set the led duration
 * Valid values are:
 * 0: No LED
 * 1: 1 second
 * 2: 3 seconds
 * 3: 5 seconds
 */
#define MOVEMENT_DEFAULT_LED_DURATION 1

/* Sets how steps are counted when on the clock_face
 * Valid values are:
 * MOVEMENT_SC_OFF: Don't count steps on clock_face
 * MOVEMENT_SC_ALWAYS: Always count steps on clock_face
 * MOVEMENT_SC_DAYTIME: Count steps between MOVEMENT_STEP_COUNT_START and MOVEMENT_STEP_COUNT_END
 * MOVEMENT_SC_NOT_INSTALLED: The LIS2DW isn't installed (the code handles this without it needing to be manally set)
 */
#define MOVEMENT_DEFAULT_COUNT_STEPS MOVEMENT_SC_OFF

/* If the settings are set to use this start and end hor,
    We only count steps when the step counter face is on.
*/
#define MOVEMENT_STEP_COUNT_START 5
#define MOVEMENT_STEP_COUNT_END 22

/* Optionally debounce button presses (disable by default).
 * A value of 4 is a good starting point if you have issues
 * with multiple button presses firing.
*/
#define MOVEMENT_DEBOUNCE_TICKS 4

#endif // MOVEMENT_CONFIG_H_
