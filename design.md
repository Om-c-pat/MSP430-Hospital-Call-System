# Design details

Flowcharts for the interrupt service routines, the C scheduler and the assembly timestamp conversion. GitHub renders these diagrams directly.

## C: Timer_A0 ISR

The ISR does a small, fixed amount of work and leaves all processing to the main loop.

```mermaid
flowchart TD
    A(["Timer_A0 reaches CCR0 (every 1 ms)"]) --> B["system_milliseconds++"]
    B --> C{"pending_ticks < UINT16_MAX?"}
    C -->|yes| D["pending_ticks++"]
    C -->|no| E["hold at maximum"]
    D --> F["clear LPM0 bits in saved SR"]
    E --> F
    F --> G(["RETI: main loop resumes"])
```

## C: main-loop scheduler

```mermaid
flowchart TD
    A(["scheduler loop"]) --> B["take pending ticks<br/>(interrupts off)"]
    B --> C{"ticks == 0?"}
    C -->|yes| D["enter LPM0 with GIE"]
    D --> A
    C -->|no| E["enable interrupts"]
    E --> F{"ticks > 0?"}
    F -->|no| A
    F -->|yes| G["ticks--"]
    G --> H["read S1, S2"]
    H --> I["debounce_tick_1ms()"]
    I --> J{"DB_B2_PRESS?"}
    J -->|yes| K["process_event(B2_PRESSED)"]
    J -->|no| L{"DB_B1_PRESS?"}
    L -->|yes| M["process_event(B1_PRESSED)"]
    L -->|no| N["no button event"]
    K --> O["process_event(TIMER)<br/>update countdowns and LEDs"]
    M --> O
    N --> O
    O --> F
```

## C: UART TX ISR

```mermaid
flowchart TD
    A(["USCI_A0 interrupt, UCA0IV = 4"]) --> B{"tx_tail != tx_head?"}
    B -->|no| C["UCA0IE &= ~UCTXIE<br/>tx_active = 0"]
    B -->|yes| D["UCA0TXBUF = tx_buffer[tx_tail]"]
    D --> E["tx_tail = (tx_tail + 1) & MASK"]
    E --> F{"tx_waiting?"}
    F -->|yes| G["tx_waiting = 0<br/>clear LPM0 bits to wake main"]
    F -->|no| H["no wake-up"]
    C --> Z(["RETI"])
    G --> Z
    H --> Z
```

When the buffer is full, `send_message()` sets `tx_waiting` and sleeps in LPM0. The ISR wakes it as soon as one slot is free.

## Assembly: UART TX ISR

```mermaid
flowchart TD
    A(["USCI_A0 interrupt"]) --> B["save R14, R15"]
    B --> C{"UCTXIFG set?"}
    C -->|no| Z
    C -->|yes| D{"tx_tail != tx_head?"}
    D -->|no| E["buffer empty:<br/>clear UCTXIE, tx_active = 0"]
    D -->|yes| F["UCA0TXBUF = tx_buffer[tx_tail]<br/>(clears UCTXIFG)"]
    F --> G["tx_tail + 1"]
    G --> H{"tx_tail = TX_SIZE?"}
    H -->|yes| I["tx_tail = 0"]
    H -->|no| Z
    I --> Z
    E --> Z(["restore R15, R14, RETI"])
```

R14 and R15 are the only registers this ISR modifies, so they are the only ones saved. The enqueue routine runs only inside the timer ISR (or before GIE is set), so this ISR can never interrupt a buffer update, and no extra interrupt masking is needed.

## Assembly: timestamped record

Each record is queued as `[nnnnnn ms] <message>` followed by CR LF. The MSP430 has no divide instruction, so the 32-bit millisecond count is converted to decimal by repeated subtraction.

```mermaid
flowchart TD
    A(["uart_record"]) --> B["queue '['"]
    B --> C["load 32-bit time (ms_hi:ms_lo)<br/>point to 10^9 in pow10_table"]
    C --> D["digit = 0"]
    D --> E{"time >= power of ten?"}
    E -->|yes| F["time = time - power (SUB, SUBC)<br/>digit + 1"]
    F --> E
    E -->|no| G{"digit != 0, a digit already sent,<br/>or power <= 10^5?"}
    G -->|yes| H["queue digit"]
    G -->|no| I["skip leading zero"]
    H --> J["next power of ten"]
    I --> J
    J --> K{"powers remaining?"}
    K -->|yes| D
    K -->|no| L["queue ' ms] ', message, CR, LF"]
    L --> M(["return"])
```

Leading zeros above 10^5 are suppressed, so at least six digits are always printed. The count is advanced every tick with `ADD` and `ADDC`. It widens to seven digits after 999 999 ms (about 16.7 minutes) and wraps after about 49.7 days.

## Timing and power notes

- **Interrupt latency (assembly).** Each record takes roughly 0.5 ms to format at 8 MHz. A tick that produces two records can therefore overrun the 1 ms period. The CCR0 flag stays pending in that case, so the next tick is serviced late rather than lost, and the timestamp stays correct.
- **TX buffer sizing (assembly).** The longest record is 63 characters, and the largest single-tick burst (a progress record plus an emergency-start record) is about 104. The link drains about 960 characters per second, so the 255 usable slots make dropped characters unlikely.
- **Idle cost.** An idle tick of the assembly ISR is about 130 cycles, roughly 17 µs at 8 MHz, so the CPU is active for under 2 % of each 1 ms period.
