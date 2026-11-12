
#include "state_machine.h"
#include "../Drivers/uart.h"
#include "../Drivers/Pin_Assignment.h"
#include "../Drivers/timer.h"
#include <stdio.h>




//------------------------------------------------------------//
//                    Initialise FSM                          //
//------------------------------------------------------------//

void state_machine_init(state_machine_data_t *data)
{
    data->current_state = STATE_IDLE;

    data->current_led = 0;
    data->emergency_led = 0;

    data->time_remaining = 0;
    data->t_emergency_blink = 0;

    data->normal_suspend = false;

    all_leds_off();
}

//------------------------------------------------------------//
//                    Event log                               //
//------------------------------------------------------------//
static void log_event(const char *event)
{
    uint32_t ms = timer_get_millis();

    char digits[11];
    unsigned int i = 0;
    unsigned int j;
    char temp;

    // Convert milliseconds to ASCII digits
    do
    {
        digits[i++] = '0' + (ms % 10);
        ms /= 10;
    }
    while (ms > 0);

    // Ensure at least 5 digits
    while (i < 5)
    {
        digits[i++] = '0';
    }

    // Reverse digits into correct order
    for (j = 0; j < i / 2; j++)
    {
        temp = digits[j];
        digits[j] = digits[i - 1 - j];
        digits[i - 1 - j] = temp;
    }

    digits[i] = '\0';

    // Transmit event record
    send_message("[ ");
    send_message(digits);
    send_message(" ms ] -> ");
    send_message(event);
    send_message("\r\n");
}
//------------------------------------------------------------//
//                    Enter State Logic                       //
//------------------------------------------------------------//

static void state_enter(state_machine_data_t *data, state_e to)
{
    state_e from = data->current_state;

    if (from == to)
    {
        return;
    }

    //--------------------ENTER EMERGENCY---------------------//

    if (to == STATE_EMERGENCY)
    {
        if (from == STATE_NORMAL)
        {
            // Suspend NORMAL operation
            data->normal_suspend = true;

            // Preserve current_led and time_remaining

            // Keep the active NORMAL LED illuminated
            // while EMERGENCY is operating
        }
        else
        {
            // Entering EMERGENCY from IDLE
            normal_leds_off();
            data->normal_suspend = false;
        }

        // Initialise EMERGENCY blinking
        data->t_emergency_blink = 100;

        // Emergency LEDs initially ON
        data->emergency_led = 1;
        emergency_leds_on();
    }

    //----------------------ENTER NORMAL----------------------//

    else if (to == STATE_NORMAL)
    {
        if (from == STATE_EMERGENCY && data->normal_suspend)
        {
            // Stop emergency LEDs
            emergency_leds_off();

            // NORMAL LED remains at its previous position
            // current_led and time_remaining are preserved

            data->normal_suspend = false;
        }
        else
        {
            // Start a new NORMAL sequence
            normal_leds_off();

            data->normal_suspend = false;
            data->current_led = 1;
            data->time_remaining = 500;

            LED_1_ON;
        }
    }

    //-----------------------ENTER IDLE-----------------------//

    else if (to == STATE_IDLE)
    {
        // Turn off all LEDs
        all_leds_off();

        // Clear suspended NORMAL condition
        data->normal_suspend = false;

        data->current_led = 0;
        data->emergency_led = 0;

        data->time_remaining = 0;
        data->t_emergency_blink = 0;
    }

    // Update the FSM state
    data->current_state = to;
}



 //------------------------------------------------------------//
 //                       Process Events                       //
 //------------------------------------------------------------//

void process_event(state_machine_data_t *data,
                   state_event_e next_event)
{
    //--------------------PROCESS IDLE-----------------------//

    if (data->current_state == STATE_IDLE)
    {
        if (next_event == B1_PRESSED)
        {
            state_enter(data, STATE_NORMAL);

            log_event("S1 -> NORMAL CALL START; LED = 1");
        }

        else if (next_event == B2_PRESSED)
        {
            state_enter(data, STATE_EMERGENCY);

            log_event("S2 -> EMERGENCY CALL START");
        }
    }

    //--------------------PROCESS NORMAL---------------------//

    else if (data->current_state == STATE_NORMAL)
    {
        if (next_event == B1_PRESSED)
        {
            // Acknowledge NORMAL
            state_enter(data, STATE_IDLE);

            log_event("S1 -> NORMAL CALL COMPLETE; ENTER IDLE");
        }

        else if (next_event == B2_PRESSED)
        {
            // Interrupt NORMAL
            state_enter(data, STATE_EMERGENCY);

            log_event("S2 -> EMERGENCY CALL START; NORMAL CALL SUSPEND");
        }

        else if (next_event == TIMER)
        {
            // Decrease NORMAL countdown every 1 ms
            if (data->time_remaining > 0)
            {
                data->time_remaining--;
            }

            // Advance LED when countdown expires
            if (data->time_remaining == 0)
            {
                switch (data->current_led)
                {
                    case 1:
                        LED_1_OFF;
                        LED_2_ON;
                        data->current_led = 2;

                        log_event("NORMAL CALL PROGRESS; LED = 2");
                        break;

                    case 2:
                        LED_2_OFF;
                        LED_3_ON;
                        data->current_led = 3;

                        log_event("NORMAL CALL PROGRESS; LED = 3");
                        break;

                    case 3:
                        LED_3_OFF;
                        LED_4_ON;
                        data->current_led = 4;

                        log_event("NORMAL CALL PROGRESS; LED = 4");
                        break;

                    case 4:
                        LED_4_OFF;
                        LED_1_ON;
                        data->current_led = 1;

                        log_event("NORMAL CALL PROGRESS; LED = 1");
                        break;

                    default:
                        normal_leds_off();
                        LED_1_ON;
                        data->current_led = 1;

                        log_event("NORMAL CALL PROGRESS; LED = 1");
                        break;
                }

                // Restart NORMAL countdown
                data->time_remaining = 500;
            }
        }
    }

    //-------------------PROCESS EMERGENCY--------------------//

    else if (data->current_state == STATE_EMERGENCY)
    {
        if (next_event == TIMER)
        {
            // Decrease EMERGENCY countdown
            if (data->t_emergency_blink > 0)
            {
                data->t_emergency_blink--;
            }

            // Toggle emergency LEDs
            if (data->t_emergency_blink == 0)
            {
                switch (data->emergency_led)
                {
                    case 1:
                        // Emergency LEDs currently ON
                        emergency_leds_off();
                        data->emergency_led = 2;
                        break;

                    case 2:
                        // Emergency LEDs currently OFF
                        emergency_leds_on();
                        data->emergency_led = 1;
                        break;

                    default:
                        emergency_leds_on();
                        data->emergency_led = 1;
                        break;
                }

                // Restart EMERGENCY countdown
                data->t_emergency_blink = 100;
            }
        }

        else if (next_event == B1_PRESSED)
        {
            // B1 is ignored during EMERGENCY
            log_event("S1 IGNORED: EMERGENCY CALL ACTIVE");
        }

        else if (next_event == B2_PRESSED)
        {
            // Acknowledge EMERGENCY
            if (data->normal_suspend)
            {
                // Resume interrupted NORMAL sequence
                state_enter(data, STATE_NORMAL);

                log_event("S2 -> EMERGENCY CALL COMPLETE; NORMAL CALL RESUME");
            }
            else
            {
                // Return to IDLE
                state_enter(data, STATE_IDLE);

                log_event("S2 -> EMERGENCY CALL COMPLETE; ENTER IDLE");
            }
        }
    }
}
