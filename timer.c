

#include <msp430.h>
#include <stdint.h>
#include "timer.h"

//------------------------------------------------------------//
//                       Timer data                           //
//------------------------------------------------------------//

// Number of pending timer ticks
static volatile uint16_t pending_ticks = 0;

// Milliseconds elapsed since timer initialisation
static volatile uint32_t system_milliseconds = 0;

//------------------------------------------------------------//
//                    Initialise Timer_A0                     //
//------------------------------------------------------------//

void init_timer(void)
{
    //--------------------------------------------------------//
    // 1. Configure FR5739 clock system
    //--------------------------------------------------------//

    // Unlock clock system using 16-bit register access
    CSCTL0 = CSKEY;

    // Configure DCO to approximately 8 MHz
    CSCTL1 = DCOFSEL_3;

    // Select clock sources
    // ACLK  = VLOCLK
    // SMCLK = DCOCLK
    // MCLK  = DCOCLK
    CSCTL2 = SELA__VLOCLK |
             SELS__DCOCLK |
             SELM__DCOCLK;

    // Configure dividers
    // SMCLK = 8 MHz / 8 = approximately 1 MHz
    // MCLK  = 8 MHz / 8 = approximately 1 MHz
    CSCTL3 = DIVA__1 | DIVS__8 | DIVM__8;

   

    //--------------------------------------------------------//
    // 2. Configure Timer_A0
    //--------------------------------------------------------//

    // Stop and clear Timer_A0
    // Use SMCLK as clock source
    TA0CTL = TASSEL__SMCLK | MC__STOP | TACLR;

    // Approximately 1 ms at 1 MHz SMCLK
    // 1000 clock cycles per interrupt
    TA0CCR0 = 999;

    // Enable Timer_A0 CCR0 interrupt
    TA0CCTL0 = CCIE;

    //--------------------------------------------------------//
    // 3. Initialise counters
    //--------------------------------------------------------//

    pending_ticks = 0;
    system_milliseconds = 0;

    //--------------------------------------------------------//
    // 4. Start Timer_A0
    //--------------------------------------------------------//

    TA0CTL = TASSEL__SMCLK | MC__UP | TACLR;
}

//------------------------------------------------------------//
//                    Timer_A0 CCR0 ISR                       //
//------------------------------------------------------------//

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A0_ISR(void)
{
    // Increment elapsed milliseconds
    system_milliseconds++;

    // Record pending tick without overflow
    if (pending_ticks < UINT16_MAX)
    {
        pending_ticks++;
    }

    // Wake CPU from LPM0
    __bic_SR_register_on_exit(LPM0_bits);
}

//------------------------------------------------------------//
//                Retrieve pending timer ticks                //
//------------------------------------------------------------//

uint16_t timer_take_ticks(void)
{
    uint16_t ticks;
    uint16_t previous_sr = __get_SR_register();

    // Protect shared data
    __disable_interrupt();

    ticks = pending_ticks;
    pending_ticks = 0;

    // Restore previous interrupt state
    if (previous_sr & GIE)
    {
        __enable_interrupt();
    }

    return ticks;
}

//------------------------------------------------------------//
//                 Read elapsed milliseconds                  //
//------------------------------------------------------------//

uint32_t timer_get_millis(void)
{
    uint32_t milliseconds;
    uint16_t previous_sr = __get_SR_register();

    // Protect 32-bit read on 16-bit MSP430
    __disable_interrupt();

    milliseconds = system_milliseconds;

    // Restore previous interrupt state
    if (previous_sr & GIE)
    {
        __enable_interrupt();
    }

    return milliseconds;
}
