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
 *
 * Sunrise/sunset calculations are public domain code by Paul Schlyter, December 1992
 *
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sunrise_sunset_face.h"
#include "watch.h"
#include "watch_utility.h"
#include "watch_common_display.h"
#include "filesystem.h"
#include "sunriset.h"
#include "delay.h" // Dodano do użycia delay_ms

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif



/* Play background alarm */ // skopiowano z deadline face
static void _background_alarm_play(sunrise_sunset_state_t *state)
{
            // Alarm! Sygnał 10 minut przed wschodem/zachodem
            static int8_t beep_sequence[] = {
            BUZZER_NOTE_A6, 5,
            BUZZER_NOTE_REST, 4,
            BUZZER_NOTE_C7, 5,
            BUZZER_NOTE_REST, 4,
            BUZZER_NOTE_D7SHARP_E7FLAT, 10,
            BUZZER_NOTE_REST, 2,
            BUZZER_NOTE_E7, 5,
            0
            };
            movement_play_sequence(beep_sequence, 0);
            //movement_play_alarm();
            // Opcjonalnie: Włącz diodę LED na chwilę, aby była widoczna sygnalizacja.
            watch_set_led_red();
            watch_set_indicator(WATCH_INDICATOR_BELL);
            delay_ms(100);
            watch_set_led_off();
            watch_clear_indicator(WATCH_INDICATOR_BELL);
            delay_ms(100);
            watch_set_led_red();
            watch_set_indicator(WATCH_INDICATOR_BELL);
            delay_ms(100);
            watch_set_led_off();
            watch_clear_indicator(WATCH_INDICATOR_BELL);
            //movement_illuminate_led();
          
            //movement_move_to_page(movement_face_to_page(state->page));
            movement_move_to_page(movement_face_to_page(state->face_idx));
            
}

static inline void button_beep() {
    // play a beep as confirmation for a button press (if applicable)
    if (movement_button_should_sound()) watch_buzzer_play_note_with_volume(BUZZER_NOTE_C7, 50, movement_button_volume());
}



static const uint8_t _location_count = sizeof(longLatPresets) / sizeof(long_lat_presets_t);
static void persist_location_to_filesystem(movement_location_t new_location) {
    movement_location_t maybe_location = {0};

    filesystem_read_file("location.u32", (char *) &maybe_location.reg, sizeof(movement_location_t));
    if (new_location.reg != maybe_location.reg) {
        filesystem_write_file("location.u32", (char *) &new_location.reg, sizeof(movement_location_t));
    }
}

static movement_location_t load_location_from_filesystem() {
    movement_location_t location = {0};

    filesystem_read_file("location.u32", (char *) &location.reg, sizeof(movement_location_t));

    return location;
}

static void _sunrise_sunset_set_expiration(sunrise_sunset_state_t *state, watch_date_time_t next_rise_set) {
    uint32_t timestamp = watch_utility_date_time_to_unix_time(next_rise_set, 0);
    state->rise_set_expires = watch_utility_date_time_from_unix_time(timestamp + 60, 0);
}

// NOWA FUNKCJA: Oblicza najbliższy czas Wschodu lub Zachodu słońca.
static watch_date_time_t _sunrise_sunset_face_get_next_event(sunrise_sunset_state_t *state) {
    double rise, set, minutes;
    movement_location_t movement_location;
    watch_date_time_t next_rise_dt = {0};
    watch_date_time_t next_set_dt = {0};
    uint32_t next_rise_ts = UINT32_MAX;
    uint32_t next_set_ts = UINT32_MAX;

    if (state->longLatToUse == 0 || _location_count <= 1)
        movement_location = load_location_from_filesystem();
    else{
        movement_location.bit.latitude = longLatPresets[state->longLatToUse].latitude;
        movement_location.bit.longitude = longLatPresets[state->longLatToUse].longitude;
    }

    if (movement_location.reg == 0) {
        return (watch_date_time_t) { .reg = 0 };
    }

    watch_date_time_t date_time = movement_get_local_date_time();
    watch_date_time_t utc_now = watch_utility_date_time_convert_zone(date_time, movement_get_current_timezone_offset(), 0);
    watch_date_time_t scratch_time;
    scratch_time.reg = utc_now.reg;

    int16_t lat_centi = (int16_t)movement_location.bit.latitude;
    int16_t lon_centi = (int16_t)movement_location.bit.longitude;

    double lat = (double)lat_centi / 100.0;
    double lon = (double)lon_centi / 100.0;
    double hours_from_utc = ((double)movement_get_current_timezone_offset()) / 3600.0;
    
    // Pętla na 2 dni (dzisiaj i jutro)
    for(int i = 0; i < 2; i++) {
        uint8_t result = sun_rise_set(scratch_time.unit.year + WATCH_RTC_REFERENCE_YEAR, scratch_time.unit.month, scratch_time.unit.day, lon, lat, &rise, &set);

        if (result != 0) {
            // Dzień/noc polarna
        } else {
            // Obliczanie czasu wschodu
            double local_rise = rise + hours_from_utc;
            minutes = 60.0 * fmod(local_rise, 1);
            
            watch_date_time_t rise_dt = scratch_time;
            rise_dt.unit.hour = floor(local_rise);
            //seconds nie jest potrzebny do godziny
            if (fmod(minutes, 1) < 0.5) rise_dt.unit.minute = floor(minutes);
            else rise_dt.unit.minute = ceil(minutes);

            // Poprawka na przekroczenie 24h
            if (rise_dt.unit.minute == 60) { rise_dt.unit.minute = 0; rise_dt.unit.hour = (rise_dt.unit.hour + 1) % 24; }
            while (rise_dt.unit.hour >= 24) { 
                rise_dt.unit.hour -= 24; 
                uint32_t ts = watch_utility_date_time_to_unix_time(rise_dt, 0);
                ts += 86400; // Dodaj dzień
                rise_dt = watch_utility_date_time_from_unix_time(ts, 0);
            }
            
            uint32_t rise_ts = watch_utility_date_time_to_unix_time(rise_dt, 0);

            if (rise_ts > watch_utility_date_time_to_unix_time(date_time, 0) && rise_ts < next_rise_ts) {
                next_rise_ts = rise_ts;
                next_rise_dt = rise_dt;
            }

            // Obliczanie czasu zachodu
            double local_set = set + hours_from_utc;
            minutes = 60.0 * fmod(local_set, 1);
            
            watch_date_time_t set_dt = scratch_time;
            set_dt.unit.hour = floor(local_set);
            if (fmod(minutes, 1) < 0.5) set_dt.unit.minute = floor(minutes);
            else set_dt.unit.minute = ceil(minutes);

            // Poprawka na przekroczenie 24h
            if (set_dt.unit.minute == 60) { set_dt.unit.minute = 0; set_dt.unit.hour = (set_dt.unit.hour + 1) % 24; }
            while (set_dt.unit.hour >= 24) { 
                set_dt.unit.hour -= 24; 
                uint32_t ts = watch_utility_date_time_to_unix_time(set_dt, 0);
                ts += 86400; // Dodaj dzień
                set_dt = watch_utility_date_time_from_unix_time(ts, 0);
            }
            
            uint32_t set_ts = watch_utility_date_time_to_unix_time(set_dt, 0);

            if (set_ts > watch_utility_date_time_to_unix_time(date_time, 0) && set_ts < next_set_ts) {
                next_set_ts = set_ts;
                next_set_dt = set_dt;
            }
        }
        
        // Przesunięcie na następny dzień
        uint32_t timestamp = watch_utility_date_time_to_unix_time(utc_now, 0);
        timestamp += 86400;
        scratch_time = watch_utility_date_time_from_unix_time(timestamp, 0);
    }
    
    // Zwróć bliższy czas
    if (next_rise_ts < next_set_ts) {
        return next_rise_dt;
    } else if (next_set_ts < next_rise_ts) {
        return next_set_dt;
    } else {
        return (watch_date_time_t) { .reg = 0 };
    }
}

//***********************
static void _display_countdown(watch_date_time_t target_dt, bool is_rise) {
// ... (Oryginalna implementacja _display_countdown pozostaje bez zmian)
    char buf[8];
    watch_date_time_t now = movement_get_local_date_time();
    uint32_t now_ts = watch_utility_date_time_to_unix_time(now, 0);
    uint32_t target_ts = watch_utility_date_time_to_unix_time(target_dt, 0);

    //watch_clear_display();
    
    // Wskaźnik tytułu i symbolu
    watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "TIL", "Lt"); 
    watch_display_text_with_fallback(WATCH_POSITION_TOP_RIGHT, is_rise ? "L " : "L ", is_rise ? "L" : "L");
    watch_set_colon();

    if (target_ts <= now_ts) {
        // Czas minął
        watch_display_text(WATCH_POSITION_BOTTOM, "DONE");
        return;
    }

    int32_t remaining_seconds = target_ts - now_ts;
    
    uint32_t remaining_minutes = remaining_seconds / 60;
    uint32_t remaining_hours = remaining_minutes / 60;
    uint32_t remaining_days = remaining_hours / 24;
    
    if (remaining_days > 0) {
        // DD:HH
        snprintf(buf, sizeof(buf), "%02lu%02luDH", remaining_days, remaining_hours % 24);
        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, buf, buf);
    } else {
        // HH:MM:SS
        snprintf(buf, sizeof(buf), "%02lu%02lu%02lu", remaining_hours, remaining_minutes % 60, remaining_seconds % 60);
        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, buf, buf);
    }
}

//******************

static void _sunrise_sunset_face_update(sunrise_sunset_state_t *state) {
    char buf[14];
    double rise, set, minutes, seconds;
    bool show_next_match = false;
    movement_location_t movement_location;
    if (state->longLatToUse == 0 || _location_count <= 1)
        movement_location = load_location_from_filesystem();
    else{
        movement_location.bit.latitude = longLatPresets[state->longLatToUse].latitude;
        movement_location.bit.longitude = longLatPresets[state->longLatToUse].longitude;
    }

    if (movement_location.reg == 0) {
        watch_display_text_with_fallback(WATCH_POSITION_TOP, "Sunri", "rI");
        watch_display_text_with_fallback(WATCH_POSITION_BOTTOM, "No LOC", "No Loc");
        return;
    }

    watch_date_time_t date_time = movement_get_local_date_time(); // the current local date / time
    watch_date_time_t utc_now = watch_utility_date_time_convert_zone(date_time, movement_get_current_timezone_offset(), 0); // the current date / time in UTC
    watch_date_time_t scratch_time; // scratchpad, contains different values at different times
    scratch_time.reg = utc_now.reg;

    // Weird quirky unsigned things were happening when I tried to cast these directly to doubles below.
    // it looks redundant, but extracting them to local int16's seemed to fix it.
    int16_t lat_centi = (int16_t)movement_location.bit.latitude;
    int16_t lon_centi = (int16_t)movement_location.bit.longitude;

    double lat = (double)lat_centi / 100.0;
    double lon = (double)lon_centi / 100.0;

    // sunriset returns the rise/set times as signed decimal hours in UTC.
    // this can mean hours below 0 or above 31, which won't fit into a watch_date_time_t struct.
    // to deal with this, we set aside the offset in hours, and add it back before converting it to a watch_date_time_t.
    double hours_from_utc = ((double)movement_get_current_timezone_offset()) / 3600.0;

    // we loop twice because if it's after sunset today, we need to recalculate to display values for tomorrow.
    for(int i = 0; i < 2; i++) {
        uint8_t result = sun_rise_set(scratch_time.unit.year + WATCH_RTC_REFERENCE_YEAR, scratch_time.unit.month, scratch_time.unit.day, lon, lat, &rise, &set);

        if (result != 0) {
            watch_clear_colon();
            watch_clear_indicator(WATCH_INDICATOR_PM);
            watch_clear_indicator(WATCH_INDICATOR_24H);
            if (result == 1) watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "SET", "SE");
            else watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "RIS", "rI");
            sprintf(buf, "%2d", scratch_time.unit.day);
            watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
            watch_display_text(WATCH_POSITION_BOTTOM, "None  ");
            return;
        }

        watch_set_colon();
        if (movement_clock_mode_24h()) watch_set_indicator(WATCH_INDICATOR_24H);

        rise += hours_from_utc;
        set += hours_from_utc;

        minutes = 60.0 * fmod(rise, 1);
        seconds = 60.0 * fmod(minutes, 1);
        scratch_time.unit.hour = floor(rise);
        scratch_time.unit.second = seconds;// aj
        
        if (seconds < 30) scratch_time.unit.minute = floor(minutes);
        else scratch_time.unit.minute = ceil(minutes);

        // Handle hour overflow from timezone conversion
        while (scratch_time.unit.hour >= 24) {
            scratch_time.unit.hour -= 24;
            // Increment day (this will be handled by the date arithmetic)
            uint32_t timestamp = watch_utility_date_time_to_unix_time(scratch_time, 0);
            timestamp += 86400;
            scratch_time = watch_utility_date_time_from_unix_time(timestamp, 0);
        }

        if (scratch_time.unit.minute == 60) {
            scratch_time.unit.minute = 0;
            scratch_time.unit.hour = (scratch_time.unit.hour + 1) % 24;
        }


//************************
            
        if (date_time.reg < scratch_time.reg) _sunrise_sunset_set_expiration(state, scratch_time);
        if (date_time.reg < scratch_time.reg || show_next_match) {
        if (state->rise_index == 0 || state->rise_index == 2 || show_next_match) { // Dodano rise_index == 2
        if (state->rise_index == 2) {
            _display_countdown(scratch_time, true);
            return;
        }

        if (state->rise_index == 0 || show_next_match) {
            if (!movement_clock_mode_24h()) {
                if (watch_utility_convert_to_12_hour(&scratch_time)) watch_set_indicator(WATCH_INDICATOR_PM);
                else watch_clear_indicator(WATCH_INDICATOR_PM);
            }
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "RIS", "rI");
            sprintf(buf, "%2d", scratch_time.unit.day);
            watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
            sprintf(buf, "%2d%02d%2s", scratch_time.unit.hour, scratch_time.unit.minute,longLatPresets[state->longLatToUse].name);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            return;
        }
    } else {
        show_next_match = true;
    }
}

        minutes = 60.0 * fmod(set, 1);
        seconds = 60.0 * fmod(minutes, 1);
        scratch_time.unit.hour = floor(set);
        if (seconds < 30) scratch_time.unit.minute = floor(minutes);
        else scratch_time.unit.minute = ceil(minutes);

        // Handle hour overflow from timezone conversion
        while (scratch_time.unit.hour >= 24) {
            scratch_time.unit.hour -= 24;
            // Increment day (this will be handled by the date arithmetic)
            uint32_t timestamp = watch_utility_date_time_to_unix_time(scratch_time, 0);
            timestamp += 86400;
            scratch_time = watch_utility_date_time_from_unix_time(timestamp, 0);
        }

        if (scratch_time.unit.minute == 60) {
            scratch_time.unit.minute = 0;
            scratch_time.unit.hour = (scratch_time.unit.hour + 1) % 24;
        }

//*************

        if (date_time.reg < scratch_time.reg) _sunrise_sunset_set_expiration(state, scratch_time);
        if (date_time.reg < scratch_time.reg || show_next_match) {
        if (state->rise_index == 0 || state->rise_index == 2 || show_next_match) { // Dodano rise_index == 2
        if (state->rise_index == 2) {
            _display_countdown(scratch_time, false);
            return;
        }
        
        if (state->rise_index == 0 || show_next_match) {
            if (!movement_clock_mode_24h()) {
                if (watch_utility_convert_to_12_hour(&scratch_time)) watch_set_indicator(WATCH_INDICATOR_PM);
                else watch_clear_indicator(WATCH_INDICATOR_PM);
            }
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "SET", "SE");
            sprintf(buf, "%2d", scratch_time.unit.day);
            watch_display_text(WATCH_POSITION_TOP_RIGHT, buf);
            sprintf(buf, "%2d%02d%2s", scratch_time.unit.hour, scratch_time.unit.minute,longLatPresets[state->longLatToUse].name);
            watch_display_text(WATCH_POSITION_BOTTOM, buf);
            return;
        }
    } else {
        show_next_match = true;
    }
}
//***************

        // it's after sunset. we need to display sunrise/sunset for tomorrow.
        uint32_t timestamp = watch_utility_date_time_to_unix_time(utc_now, 0);
        timestamp += 86400;
        scratch_time = watch_utility_date_time_from_unix_time(timestamp, 0);
    }
}

static int16_t _sunrise_sunset_face_latlon_from_struct(sunrise_sunset_lat_lon_settings_t val) {
    int16_t retval = (val.sign ? -1 : 1) *
                        (
                            val.hundreds * 10000 +
                            val.tens * 1000 +
                            val.ones * 100 +
                            val.tenths * 10 +
                            val.hundredths
                        );
    return retval;
}

static sunrise_sunset_lat_lon_settings_t _sunrise_sunset_face_struct_from_latlon(int16_t val) {
    sunrise_sunset_lat_lon_settings_t retval;

    retval.sign = val < 0;
    val = abs(val);
    retval.hundredths = val % 10;
    val /= 10;
    retval.tenths = val % 10;
    val /= 10;
    retval.ones = val % 10;
    val /= 10;
    retval.tens = val % 10;
    val /= 10;
    retval.hundreds = val % 10;

    return retval;
}

static void _sunrise_sunset_face_update_location_register(sunrise_sunset_state_t *state) {
    if (state->location_changed) {
        movement_location_t movement_location;
        int16_t lat = _sunrise_sunset_face_latlon_from_struct(state->working_latitude);
        int16_t lon = _sunrise_sunset_face_latlon_from_struct(state->working_longitude);
        movement_location.bit.latitude = lat;
        movement_location.bit.longitude = lon;
        persist_location_to_filesystem(movement_location);
        state->location_changed = false;
    }
}

static void _sunrise_sunset_face_update_settings_display(movement_event_t event, sunrise_sunset_state_t *state) {
    char buf[12];

//    watch_clear_display();

    switch (state->page) {
        case 0:
            return;
        case 1:
            // Latitude
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LAT", "LA");
            if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                watch_set_decimal_if_available();
                watch_display_character('0' + state->working_latitude.tens, 4);
                watch_display_character('0' + state->working_latitude.ones, 5);
                watch_display_character('0' + state->working_latitude.tenths, 6);
                watch_display_character('0' + state->working_latitude.hundredths, 7);
                watch_display_character('#', 8);
                if (state->working_latitude.sign) watch_display_character('S', 9);
                else watch_display_character('N', 9);

                if (event.subsecond % 2) {
                    watch_display_character(' ', 4 + state->active_digit);
                    // for degrees N or S, also flash the last character
                    if (state->active_digit == 4) watch_display_character(' ', 9);
                }
            } else {
                sprintf(buf, "%c %04d", state->working_latitude.sign ? '-' : '+', abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)));
                if (event.subsecond % 2) buf[state->active_digit] = ' ';
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
            break;
        case 2:
            // Longitude
            watch_display_text_with_fallback(WATCH_POSITION_TOP_LEFT, "LON", "LO");
            if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                watch_set_decimal_if_available();
                // Handle leading 1 for longitudes >99
                if (state->working_longitude.hundreds == 1) watch_set_pixel(0, 22);
                watch_display_character('0' + state->working_longitude.tens, 4);
                watch_display_character('0' + state->working_longitude.ones, 5);
                watch_display_character('0' + state->working_longitude.tenths, 6);
                watch_display_character('0' + state->working_longitude.hundredths, 7);
                watch_display_character('#', 8);
                if (state->working_longitude.sign) watch_display_character('W', 9);
                else watch_display_character('E', 9);
                if (event.subsecond % 2) {
                    watch_display_character(' ', 4 + state->active_digit);
                    // for tens place, also flash leading 1 if present
                    if (state->active_digit == 0) watch_clear_pixel(0, 22);
                    // for degrees E or W, also flash the last character
                    if (state->active_digit == 4) watch_display_character(' ', 9);
                }
            } else {
                sprintf(buf, "%c%05d", state->working_longitude.sign ? '-' : '+', abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)));
                if (event.subsecond % 2) buf[state->active_digit] = ' ';
                watch_display_text(WATCH_POSITION_BOTTOM, buf);
            }
            break;
    }
}

static void _sunrise_sunset_face_advance_digit(sunrise_sunset_state_t *state) {
    state->location_changed = true;
    if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
        switch (state->page) {
            case 1: // latitude
                switch (state->active_digit) {
                    case 0: // tens
                        state->working_latitude.tens = (state->working_latitude.tens + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) {
                            // prevent latitude from going over ±90.
                            // TODO: perform these checks when advancing the digit?
                            state->working_latitude.ones = 0;
                            state->working_latitude.tenths = 0;
                            state->working_latitude.hundredths = 0;
                        }
                        break;
                    case 1:
                        state->working_latitude.ones = (state->working_latitude.ones + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                        break;
                    case 2:
                        state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                        break;
                    case 3:
                        state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
                        break;
                    case 4:
                        state->working_latitude.sign++;
                        break;
                }
                break;
            case 2: // longitude
                switch (state->active_digit) {
                    case 0:
                        // Increase tens and handle carry-over to hundreds
                        state->working_longitude.tens++;
                        if (state->working_longitude.tens >= 10) {
                            state->working_longitude.tens = 0;
                            state->working_longitude.hundreds++;
                        }

                        // Reset if we've gone over ±180
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) {
                            state->working_longitude.hundreds = 0;
                            state->working_longitude.tens = 0;
                            state->working_longitude.ones = 0;
                            state->working_longitude.tenths = 0;
                            state->working_longitude.hundredths = 0;
                        }
                        break;
                    case 1:
                        state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                        break;
                    case 2:
                        state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                        break;
                    case 3:
                        state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                        break;
                    case 4:
                        state->working_longitude.sign++;
                        break;
                }
                break;
        }
    } else {
        switch (state->page) {
            case 1: // latitude
                switch (state->active_digit) {
                    case 0:
                        state->working_latitude.sign++;
                        break;
                    case 1:
                        // we skip this digit
                        break;
                    case 2:
                        state->working_latitude.tens = (state->working_latitude.tens + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) {
                            // prevent latitude from going over ±90.
                            // TODO: perform these checks when advancing the digit?
                            state->working_latitude.ones = 0;
                            state->working_latitude.tenths = 0;
                            state->working_latitude.hundredths = 0;
                        }
                        break;
                    case 3:
                        state->working_latitude.ones = (state->working_latitude.ones + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.ones = 0;
                        break;
                    case 4:
                        state->working_latitude.tenths = (state->working_latitude.tenths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.tenths = 0;
                        break;
                    case 5:
                        state->working_latitude.hundredths = (state->working_latitude.hundredths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_latitude)) > 9000) state->working_latitude.hundredths = 0;
                        break;
                }
                break;
            case 2: // longitude
                switch (state->active_digit) {
                    case 0:
                        state->working_longitude.sign++;
                        break;
                    case 1:
                        state->working_longitude.hundreds = (state->working_longitude.hundreds + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) {
                            // prevent longitude from going over ±180
                            state->working_longitude.tens = 8;
                            state->working_longitude.ones = 0;
                            state->working_longitude.tenths = 0;
                            state->working_longitude.hundredths = 0;
                        }
                        break;
                    case 2:
                        state->working_longitude.tens = (state->working_longitude.tens + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tens = 0;
                        break;
                    case 3:
                        state->working_longitude.ones = (state->working_longitude.ones + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.ones = 0;
                        break;
                    case 4:
                        state->working_longitude.tenths = (state->working_longitude.tenths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.tenths = 0;
                        break;
                    case 5:
                        state->working_longitude.hundredths = (state->working_longitude.hundredths + 1) % 10;
                        if (abs(_sunrise_sunset_face_latlon_from_struct(state->working_longitude)) > 18000) state->working_longitude.hundredths = 0;
                        break;
                }
                break;
        }
    }
}

void sunrise_sunset_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(sunrise_sunset_state_t));
        memset(*context_ptr, 0, sizeof(sunrise_sunset_state_t));
        // NOWA INICJALIZACJA:
        sunrise_sunset_state_t *state = (sunrise_sunset_state_t *) *context_ptr;
        state->alarm_enabled = false;
        state->next_event_time.reg = 0;
        state->face_idx = watch_face_index;
    }
}

void sunrise_sunset_face_activate(void *context) {
    if (watch_sleep_animation_is_running()) watch_stop_sleep_animation();

#if __EMSCRIPTEN__
    int16_t browser_lat = EM_ASM_INT({
        return lat;
    });
    int16_t browser_lon = EM_ASM_INT({
        return lon;
    });
    if ((watch_get_backup_data(1) == 0) && (browser_lat || browser_lon)) {
        movement_location_t browser_loc;
        browser_loc.bit.latitude = browser_lat;
        browser_loc.bit.longitude = browser_lon;
        watch_store_backup_data(browser_loc.reg, 1);
    }
#endif

    sunrise_sunset_state_t *state = (sunrise_sunset_state_t *)context;
    movement_location_t movement_location = load_location_from_filesystem();
    state->working_latitude = _sunrise_sunset_face_struct_from_latlon(movement_location.bit.latitude);
    state->working_longitude = _sunrise_sunset_face_struct_from_latlon(movement_location.bit.longitude);
    state->rise_index = 2;
    
    //Pokaż wskaźnik alarmu, jeśli jest włączony
    if (state->alarm_enabled) watch_set_indicator(WATCH_INDICATOR_BELL);
    else watch_clear_indicator(WATCH_INDICATOR_BELL);
}

bool sunrise_sunset_face_loop(movement_event_t event, void *context) {
    sunrise_sunset_state_t *state = (sunrise_sunset_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            _sunrise_sunset_face_update(state);
            break;
        case EVENT_LOW_ENERGY_UPDATE:
        case EVENT_TICK:
            _sunrise_sunset_face_update(state);
                if (state->page == 0) {
                // if entering low energy mode, start tick animation
                if (event.event_type == EVENT_LOW_ENERGY_UPDATE && !watch_sleep_animation_is_running()) watch_start_sleep_animation(1000);
                // check if we need to update the display
                watch_date_time_t date_time = movement_get_local_date_time();
                if (date_time.reg >= state->rise_set_expires.reg) {
                    // and on the off chance that this happened before EVENT_TIMEOUT snapped us back to rise/set 0, go back now
                    state->rise_index = 0;
                    _sunrise_sunset_face_update(state);
                }
            } else {
                _sunrise_sunset_face_update_settings_display(event, state);
            }
            break;
        case EVENT_LIGHT_BUTTON_DOWN:
            if (state->page) {
                if (watch_get_lcd_type() == WATCH_LCD_TYPE_CUSTOM) {
                    state->active_digit++;
                    if (state->active_digit > 4) {
                        state->active_digit = 0;
                        state->page = (state->page + 1) % 3;
                        _sunrise_sunset_face_update_location_register(state);
                    }
                } else {
                    state->active_digit++;
                    if (state->page == 1 && state->active_digit == 1) state->active_digit++; // max latitude is +- 90, no hundreds place
                    if (state->active_digit > 5) {
                        state->active_digit = 0;
                        state->page = (state->page + 1) % 3;
                        _sunrise_sunset_face_update_location_register(state);
                    }
                }
                _sunrise_sunset_face_update_settings_display(event, context);
            } else if (_location_count <= 1) {
                movement_illuminate_led();
            }
            if (state->page == 0) {
                movement_request_tick_frequency(1);
                _sunrise_sunset_face_update(state);
            }
            break;
        case EVENT_LIGHT_LONG_PRESS:
            // Zmieniona logika: Włączenie/wyłączenie alarmu.
            
            if (state->rise_index == 2){
            
            if (!state->page) {
                state->alarm_enabled = !state->alarm_enabled;
                if (state->alarm_enabled) {
                    watch_set_indicator(WATCH_INDICATOR_BELL);
                    // beep to confirm setting.
                    button_beep();
                } else {
                    watch_clear_indicator(WATCH_INDICATOR_BELL);
                    // beep to confirm setting.
                    button_beep();
                }
                }
                } else movement_illuminate_led(); // Zachowanie oryginalnej logiki dla <1 presetów
                //if (_location_count <= 1) movement_illuminate_led();
                
            break;
        case EVENT_LIGHT_BUTTON_UP:
            if (state->page == 0 && _location_count > 1) {
                state->longLatToUse = (state->longLatToUse + 1) % _location_count;
                _sunrise_sunset_face_update(state);
            }
            break;
        case EVENT_ALARM_BUTTON_UP:
            if (state->page) {
                _sunrise_sunset_face_advance_digit(state);
                _sunrise_sunset_face_update_settings_display(event, context);
            } else {
                _sunrise_sunset_face_update(state);
                state->rise_index++;
                if (state->rise_index > 2) {
                    state->rise_index = 0;
                }
                state->rise_index = (state->rise_index + 1) % 3;
                if (state->rise_index == 2) { // Jeśli jesteśmy na 2 (odliczanie)
                state->rise_index = 0; // Przejdź do 0 (pierwszy czas)
                } else if (state->rise_index == 0) {
                    state->rise_index = 1; // Przejdź do 1 (drugi czas)
                } else if (state->rise_index == 1) {
                    state->rise_index = 2; // Przejdź do 2 (odliczanie)
                }
                
                // ZMIANA: Cykl 3-elementowy dla ekranów 0, 1, 2
                state->rise_index = (state->rise_index + 1) % 3;
                _sunrise_sunset_face_update(state);
                
            }
            break;
        case EVENT_ALARM_LONG_PRESS:
            if (state->page == 0) {
            if (state->longLatToUse != 0) {
                state->longLatToUse = 0;
                _sunrise_sunset_face_update(state);
                break;
            }
                state->page++;
                state->active_digit = 0;
                watch_clear_display();
                movement_request_tick_frequency(4);
                _sunrise_sunset_face_update_settings_display(event, context);
            }
            else {
                state->active_digit = 0;
                state->page = 0;
                _sunrise_sunset_face_update_location_register(state);
                state->rise_index = 2;
                _sunrise_sunset_face_update(state);
            }
            break;
        case EVENT_TIMEOUT:
            if (load_location_from_filesystem().reg == 0) {
                // if no location set, return home
                movement_move_to_page(0);
            }  else if (state->page || state->rise_index) {
                 //otherwise on timeout, exit settings mode and return to the next sunrise or sunset
                state->page = 0;
                state->rise_index = 2;
                movement_request_tick_frequency(1);
                _sunrise_sunset_face_update(state);
                movement_move_to_page(0);
            }
            break;
        case EVENT_BACKGROUND_TASK:
            _background_alarm_play(state); //skopiowano z deadline face
            break;
        default:
            return movement_default_loop_handler(event);
    }

    return true;
}

void sunrise_sunset_face_resign(void *context) {
    sunrise_sunset_state_t *state = (sunrise_sunset_state_t *)context;
    state->page = 0;
    state->active_digit = 0;
    state->rise_index = 0;
    _sunrise_sunset_face_update_location_register(state);
    
     //Wyczyść wskaźnik alarmu przy rezygnacji
    //watch_clear_indicator(WATCH_INDICATOR_BELL);
}

movement_watch_face_advisory_t sunrise_sunset_face_advise(void *context) {
    sunrise_sunset_state_t *state = (sunrise_sunset_state_t *) context;
    movement_watch_face_advisory_t retval = { 0 };
    // Alarm musi być włączony
    if (!state->alarm_enabled)
        return retval;

    // Upewniamy się, że mamy aktualnie najbliższy czas zdarzenia
    state->next_event_time = _sunrise_sunset_face_get_next_event(state);

    if (state->next_event_time.reg == 0)
        return retval; // Brak zdarzeń w najbliższym czasie lub problem z lokalizacją

    // Pobranie aktualnego czasu
    watch_date_time_t now = movement_get_local_date_time();
    uint32_t now_ts = watch_utility_date_time_to_unix_time(now, 0);

    // Czas najbliższego zdarzenia
    uint32_t next_event_ts = watch_utility_date_time_to_unix_time(state->next_event_time, 0);
    
    // Czas alarmu: 10 minut (600 sekund) przed zdarzeniem
    uint32_t alarm_ts = next_event_ts > 600 ? next_event_ts - 600 : 0; 

    if (alarm_ts < now_ts)
        return retval; // Czas na alarm już minął

    // Czy czas alarmu mieści się w następnej minucie?
    if (alarm_ts < now_ts + 60) { 
        retval.wants_background_task = true;
    }

    return retval;
}


//movement_watch_face_advisory_t sunrise_sunset_face_advise(void *context) {
 //   (void) context;
 //   movement_watch_face_advisory_t retval = { 0 };

    // this will get called at the top of each minute, so all we check is if we're at the top of the hour as well.
    // if we are, we ask for a background task.
   // retval.wants_background_task = true;

  //  return retval;
//}


