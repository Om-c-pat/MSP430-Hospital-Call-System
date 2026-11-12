
#ifndef DEBOUNCE_H_
#define DEBOUNCE_H_

#include <stdbool.h>
#include <stdint.h>

// Debounce FSM states
typedef enum
{
    STABLE_HIGH,
    DEBOUNCE_PRESSED,
    STABLE_LOW,
    DEBOUNCE_RELEASE

} debounce_state_t;

// Individual button data
typedef struct
{
    debounce_state_t current_db_state;
    uint16_t db_counter;

} debounce_button_t;

// Both buttons
typedef struct
{
    debounce_button_t b1;
    debounce_button_t b2;

} debounce_data_t;

// Button events
#define DB_NO_PRESS  0x00u
#define DB_B1_PRESS  0x01u
#define DB_B2_PRESS  0x02u

void debounce_init(debounce_data_t *data);

uint8_t debounce_tick_1ms(
    debounce_data_t *data,
    bool b1_pressed,
    bool b2_pressed);

#endif