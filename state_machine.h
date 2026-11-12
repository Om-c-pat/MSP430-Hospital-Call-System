#ifndef STATE_MACHINE_H_
#define STATE_MACHINE_H_

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------
// Main FSM states
//-----------------------------------------

typedef enum {
    STATE_IDLE,
    STATE_NORMAL,
    STATE_EMERGENCY
} state_e;

//-----------------------------------------
// FSM events
//-----------------------------------------

typedef enum {
    B1_PRESSED,
    B2_PRESSED,
    TIMER
} state_event_e;

//-----------------------------------------
// FSM data
//-----------------------------------------

typedef struct state_machine_data {
    state_e current_state;

    uint8_t current_led;
    uint8_t emergency_led;
    uint16_t time_remaining;
    uint16_t t_emergency_blink;
    bool normal_suspend;
} state_machine_data_t;

//-----------------------------------------
// Public function declarations
//-----------------------------------------

void state_machine_init(state_machine_data_t *data);

void process_event(state_machine_data_t *data,
                   state_event_e next_event);

#endif