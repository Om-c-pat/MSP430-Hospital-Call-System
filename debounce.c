
#include "FSM/debounce.h"

#define DEBOUNCE_TICKS 10u

//-----------------------------------------
// Internal Function Declaration
//-----------------------------------------

static bool debounce_update_button(
    debounce_button_t *button,
    bool pressed);


//-----------------------------------------
// Initialise debounce FSMs
//-----------------------------------------

void debounce_init(debounce_data_t *data)
{
    data->b1.current_db_state = STABLE_HIGH;
    data->b1.db_counter = 0;

    data->b2.current_db_state = STABLE_HIGH;
    data->b2.db_counter = 0;
}


//-----------------------------------------
// Update one button FSM
//-----------------------------------------

static bool debounce_update_button(
    debounce_button_t *button,
    bool pressed)
{
    bool valid_press = false;

    switch (button->current_db_state)
    {
        case STABLE_HIGH:

            if (pressed)
            {
                button->current_db_state = DEBOUNCE_PRESSED;
                button->db_counter = DEBOUNCE_TICKS;
            }

            break;


        case DEBOUNCE_PRESSED:

            if (pressed)
            {
                if (button->db_counter > 0)
                {
                    button->db_counter--;
                }

                if (button->db_counter == 0)
                {
                    button->current_db_state = STABLE_LOW;
                    valid_press = true;
                }
            }
            else
            {
                button->current_db_state = STABLE_HIGH;
                button->db_counter = 0;
            }

            break;


        case STABLE_LOW:

            if (!pressed)
            {
                button->current_db_state = DEBOUNCE_RELEASE;
                button->db_counter = DEBOUNCE_TICKS;
            }

            break;


        case DEBOUNCE_RELEASE:

            if (!pressed)
            {
                if (button->db_counter > 0)
                {
                    button->db_counter--;
                }

                if (button->db_counter == 0)
                {
                    button->current_db_state = STABLE_HIGH;
                    button->db_counter = 0;
                }
            }
            else
            {
                button->current_db_state = STABLE_LOW;
                button->db_counter = 0;
            }

            break;


        default:

            button->current_db_state = STABLE_HIGH;
            button->db_counter = 0;

            break;
    }

    return valid_press;
}


//-----------------------------------------
// Update both buttons every 1 ms
//-----------------------------------------

uint8_t debounce_tick_1ms(
    debounce_data_t *data,
    bool b1_pressed,
    bool b2_pressed)
{
    uint8_t events = DB_NO_PRESS;

    // Update B1
    if (debounce_update_button(&data->b1, b1_pressed))
    {
        events |= DB_B1_PRESS;
    }

    // Update B2
    if (debounce_update_button(&data->b2, b2_pressed))
    { 
        events |= DB_B2_PRESS; 
    } 

    return events;
}
