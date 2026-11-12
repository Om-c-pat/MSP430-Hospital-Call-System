
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

#include "Drivers/Pin_Assignment.h"
#include "Drivers/timer.h"
#include "Drivers/uart.h"
#include "FSM/state_machine.h"
#include "FSM/debounce.h"

//------------------------------------------------------------//
//                         MAIN                               //
//------------------------------------------------------------//

int main(void)
{
    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Disable interrupts during initialisation
    __disable_interrupt();

    //--------------------------------------------------------//
    // 1. Create program data
    //--------------------------------------------------------//

    state_machine_data_t controller;
    debounce_data_t debounce;

    //--------------------------------------------------------//
    // 2. Initialise hardware
    //--------------------------------------------------------//

    // Configure FR5739 LEDs:
    // LED 1-4: PJ.0 - PJ.3
    // LED 5-8: P3.4 - P3.7
    init_pins();

    // Configure FR5739 buttons:
    // S1: P4.0
    // S2: P4.1
    init_buttons();

    //--------------------------------------------------------//
    // 3. Initialise FSMs
    //--------------------------------------------------------//

    state_machine_init(&controller);
    debounce_init(&debounce);

    //--------------------------------------------------------//
    // 4. Initialise Timer_A0 and clock
    //--------------------------------------------------------//

    // Configure approximately 1 MHz SMCLK
    // and Timer_A0 1 ms interrupt
    init_timer();

    //--------------------------------------------------------//
    // 5. Initialise UART
    //--------------------------------------------------------//

    // FR5739 application UART:
    // UCA0 TX: P2.0
    // UCA0 RX: P2.1
    // 9600 baud, 8N1
    init_UART();

    //--------------------------------------------------------//
    // 6. Enable interrupts
    //--------------------------------------------------------//

    // Enables Timer_A0 and UART TX ISRs
    __enable_interrupt();

    // Transmit startup message
    send_message("[ 00000 ms ] -> SYSTEM READY; IDLE\r\n");

    //--------------------------------------------------------//
    // 7. Main event loop
    //--------------------------------------------------------//

    while (1)
    {
        uint16_t ticks;

        // Disable interrupts while retrieving pending ticks
        __disable_interrupt();

        ticks = timer_take_ticks();

        //----------------------------------------------------//
        // Enter LPM0 when there are no pending ticks
        //----------------------------------------------------//

        if (ticks == 0)
        {
            // Enable interrupts and enter LPM0 atomically
            __bis_SR_register(LPM0_bits | GIE);

            // Resume execution after interrupt
            __no_operation();

            continue;
        }

        // Enable interrupts during event processing
        __enable_interrupt();

        //----------------------------------------------------//
        // Process pending 1 ms ticks
        //----------------------------------------------------//

        while (ticks > 0)
        {
            ticks--;

            //-----------------------------------------------//
            // 8. Read physical buttons
            //-----------------------------------------------//

            // FR5739 onboard switches:
            // S1 = P4.0
            // S2 = P4.1
            // Active-low: LOW means pressed

            bool b1_pressed = ((P4IN & BIT0) == 0);
            bool b2_pressed = ((P4IN & BIT1) == 0);

            //-----------------------------------------------//
            // 9. Update debounce FSM
            //-----------------------------------------------//

            uint8_t button_events = debounce_tick_1ms(
                &debounce,
                b1_pressed,
                b2_pressed
            );

            //-----------------------------------------------//
            // 10. Process validated button events
            //-----------------------------------------------//

            // Emergency S2 has priority over S1

            if (button_events & DB_B2_PRESS)
            {
                process_event(&controller, B2_PRESSED);
            }
            else if (button_events & DB_B1_PRESS)
            {
                process_event(&controller, B1_PRESSED);
            }

            //-----------------------------------------------//
            // 11. Update FSM timers
            //-----------------------------------------------//

            process_event(&controller, TIMER);
        }
    }
}
