#include <msp430.h>
#include <stdint.h>
#include "uart.h"

//------------------------------------------------------------//
//                    UART TX Buffer                          //
//------------------------------------------------------------//

#define UART_TX_BUFFER_SIZE 256u
#define UART_TX_BUFFER_MASK (UART_TX_BUFFER_SIZE - 1u)

// Circular transmit buffer
static volatile char tx_buffer[UART_TX_BUFFER_SIZE];

// Buffer indexes
static volatile uint16_t tx_head = 0;
static volatile uint16_t tx_tail = 0;

// UART transmission status
static volatile uint8_t tx_active = 0;

// Indicates main is waiting for buffer space
static volatile uint8_t tx_waiting = 0;

//------------------------------------------------------------//
//                    Send UART Message                       //
//------------------------------------------------------------//

// Called from main/application code with interrupts enabled.
// Characters are queued and transmitted by the UART TX ISR.

void send_message(const char *str)
{
    while (*str != '\0')
    {
        uint16_t next_head;

        // Protect shared UART buffer data
        __disable_interrupt();

        next_head =
            (tx_head + 1u) & UART_TX_BUFFER_MASK;

        //----------------------------------------------------//
        // Buffer has available space
        //----------------------------------------------------//

        if (next_head != tx_tail)
        {
            // Add character to circular buffer
            tx_buffer[tx_head] = *str++;

            // Update write index
            tx_head = next_head;

            //------------------------------------------------//
            // Start UART transmission if inactive
            //------------------------------------------------//

            if (tx_active == 0)
            {
                tx_active = 1;

                // Load first character directly
                UCA0TXBUF = tx_buffer[tx_tail];

                // Advance read index
                tx_tail =
                    (tx_tail + 1u) & UART_TX_BUFFER_MASK;

                // Enable UART TX interrupt
                UCA0IE |= UCTXIE;
            }

            // Restore global interrupts
            __enable_interrupt();
        }

        //----------------------------------------------------//
        // Buffer full
        //----------------------------------------------------//

        else
        {
            // Indicate that main requires buffer space
            tx_waiting = 1;

            // Sleep until an interrupt wakes the CPU
            __bis_SR_register(LPM0_bits | GIE);
            __no_operation();
        }
    }
}

//------------------------------------------------------------//
//                     Initialise UART                        //
//------------------------------------------------------------//

void init_UART(void)
{
    //--------------------------------------------------------//
    // UART A0: P2.0 TX, P2.1 RX
    //--------------------------------------------------------//

    P2SEL1 |= BIT0 | BIT1;
    P2SEL0 &= ~(BIT0 | BIT1);

    //--------------------------------------------------------//
    // Hold UART in reset and select SMCLK
    //--------------------------------------------------------//

    UCA0CTLW0 = UCSWRST | UCSSEL__SMCLK;

    //--------------------------------------------------------//
    // Baud rate: 9600 baud
    // SMCLK = approximately 1 MHz
    //--------------------------------------------------------//

    UCA0BRW = 6;
    UCA0MCTLW = 0x2081;

    //--------------------------------------------------------//
    // Initialise UART buffer
    //--------------------------------------------------------//

    tx_head = 0;
    tx_tail = 0;

    tx_active = 0;
    tx_waiting = 0;

    //--------------------------------------------------------//
    // Configure UART interrupts
    //--------------------------------------------------------//

    // Disable TX and RX interrupts initially
    UCA0IE &= ~(UCTXIE | UCRXIE);

    // Release UART from reset
    UCA0CTLW0 &= ~UCSWRST;
}

//------------------------------------------------------------//
//                UART A0 Interrupt Service Routine          //
//------------------------------------------------------------//

#pragma vector = USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
{
    switch (__even_in_range(UCA0IV, 0x08))
    {
        //----------------------------------------------------//
        // No interrupt
        //----------------------------------------------------//

        case 0:
            break;

        //----------------------------------------------------//
        // UART RX interrupt
        //----------------------------------------------------//

        case 2:
            // RX not used in this application
            break;

        //----------------------------------------------------//
        // UART TX interrupt
        //----------------------------------------------------//

        case 4:

            // Check whether characters remain
            if (tx_tail != tx_head)
            {
                // Send next queued character
                UCA0TXBUF = tx_buffer[tx_tail];

                // Advance read index
                tx_tail =
                    (tx_tail + 1u) & UART_TX_BUFFER_MASK;

                //--------------------------------------------//
                // Wake main if waiting for buffer space
                //--------------------------------------------//

                if (tx_waiting)
                {
                    tx_waiting = 0;

                    __bic_SR_register_on_exit(LPM0_bits);
                }
            }
            else
            {
                //--------------------------------------------//
                // No characters remaining
                //--------------------------------------------//

                // Disable transmit interrupt
                UCA0IE &= ~UCTXIE;

                // Mark UART as inactive
                tx_active = 0;
            }

            break;

        default:
            break;
    }
}