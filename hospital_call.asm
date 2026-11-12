    .cdecls C,LIST,"msp430.h"

    .def    RESET
    .global __STACK_END

TX_SIZE .equ    256                     ; UART: size of the transmit buffer

    .bss    timer_ticking,2,2
    
    .bss    s1_down_count,2,2
    .bss    s2_down_count,2,2
    .bss    s1_up_count,2,2
    .bss    s2_up_count,2,2
    .bss    s1_new_press,2,2
    .bss    s2_new_press,2,2
    .bss    s1_press,2,2
    .bss    s2_press,2,2
    
    .bss    normal,2,2
    .bss    normal_count,2,2
    .bss    normal_led,2,2

    .bss    emergency,2,2
    .bss    emergency_count,2,2  

    .bss    tx_buffer,TX_SIZE           ; UART: characters waiting to be sent
    .bss    tx_head,2,2                 ; UART: where the next character is put
    .bss    tx_tail,2,2                 ; UART: where the next character is sent from
    .bss    tx_active,2,2               ; UART: 1 = UART is busy sending
    .bss    ms_lo,2,2                   ; UART: millisecond timestamp (low word)
    .bss    ms_hi,2,2                   ; UART: millisecond timestamp (high word)
    .bss    ur_started,2,2              ; UART: 1 once a non-zero digit is sent

    .text
    .retain
    .retainrefs




RESET:
    MOV.W   #__STACK_END, SP
    MOV.W   #WDTPW|WDTHOLD, &WDTCTL

    CALL    #init_clock                 ; UART: SMCLK = 1 MHz (9600 baud), CPU = 8 MHz


    MOV.W   #0, &s1_press
    MOV.W   #0, &s2_press
    MOV.W   #0, &timer_ticking


    MOV.W   #0, &s1_down_count
    MOV.W   #0, &s1_up_count
    MOV.W   #0, &s2_down_count
    MOV.W   #0, &s2_up_count
    MOV.W   #0, &s1_new_press
    MOV.W   #0, &s2_new_press

    MOV.W   #0, &normal
    MOV.W   #0, &normal_count
    MOV.W   #0x01, &normal_led
    MOV.W   #0, &emergency
    MOV.W   #0, &emergency_count



    BIC.W   #0x0F, &PJOUT     ; LED 1-4 OFF
    BIS.W   #0x0F, &PJDIR     ; LED 1-4 OUTPUTS

    BIC.B   #0xF0, &P3OUT     ; led 5-8 OFF
    BIS.B   #0xF0, &P3DIR     ; 5-8 OUTPUTS


    BIC.B   #0x03, &P4DIR     ; S1 AND S2 inputs

    MOV.W   #TACLR, &TA0CTL     ; stops and clears timer

    BIS.W   #TASSEL1, &TA0CTL    ; selects SMCLK, bit TASSEL1 = 1
    BIC.W   #TASSEL0, &TA0CTL    ; bit TASSEL0 = 0

    BIS.W   #ID1, &TA0CTL       ;Select input divider = 8, bit id1 = 1
    BIS.W   #ID0, &TA0CTL       ;Select input divider = 8, bit id0 = 1

    MOV.W   #124, &TA0CCR0      ;set 124 to register CCR0, this sets counting limit
    MOV.W   #CCIE, &TA0CCTL0      ; enables CCR0 interrupt

    BIC.W   #MC1, &TA0CTL          ;select up mode for timer a0, bit MC1 = 0
    BIS.W   #MC0, &TA0CTL          ;select up mode for timer a0, bit MC0 = 1

    
    


    MOV.W   #0, &ms_lo                  ; UART: timestamp starts at 0 ms
    MOV.W   #0, &ms_hi
    CALL    #init_uart                  ; UART: 9600 baud, 8N1 on P2.0 / P2.1
    MOV.W   #msg_ready, R12             ; UART: send "SYSTEM READY; IDLE"
    CALL    #uart_record

loop:
    BIS.W  #LPM0 + GIE, SR         ;Set GIE to enable interrupt globally and enter low power mode
    NOP
    JMP     loop


timer_A0:
    PUSH.W  R12                         ; UART: save registers the UART code uses
    PUSH.W  R13
    PUSH.W  R14
    PUSH.W  R15
    ADD.W   #1, &ms_lo                  ; UART: count 1 ms for the timestamp
    ADDC.W  #0, &ms_hi
    
    MOV.W   #0, &s1_new_press
    MOV.W   #0, &s2_new_press

    
    INC.W   &timer_ticking          ; runs every 1ms



;---------------------------------------------
;NORMAL CALL FLASHING LEDS
;only when normal = 1 and emergency = 0
;---------------------------------------------

    CMP.W   #1, &normal
    JNE     emergency_check
    
    CMP.W   #0, &emergency
    JNE     emergency_check
    INC.W   &normal_count
    CMP.W   #500, &normal_count
    JLO     emergency_check
    MOV.W   #0, &normal_count              
    CMP.W   #0x01, &normal_led
    JEQ     select_led2
    CMP.W   #0x02, &normal_led
    JEQ     select_led3
    CMP.W   #0x04, &normal_led
    JEQ     select_led4
    
    MOV.W   #0x01, &normal_led
    JMP     new_normal_led

select_led2:
    MOV.W   #0x02, &normal_led  
    JMP     new_normal_led

select_led3:
    MOV.W   #0x04, &normal_led  
    JMP     new_normal_led
    

select_led4:
    MOV.W   #0x08, &normal_led  
    
new_normal_led:
    BIC.W   #0x0F, &PJOUT
    BIS.W   &normal_led, &PJOUT
    CALL    #uart_progress              ; UART: "NORMAL CALL PROGRESS; LED = n"


;--------------------------------------------    
;EMEGENCY FLASHING 
;--------------------------------------
 
emergency_check:
    CMP.W   #1, &emergency
    JNE     check_s1
    INC.W   &emergency_count
    CMP.W   #200, &emergency_count
    JLO     check_s1
    MOV.W   #0, &emergency_count
    XOR.B   #0xF0, &P3OUT 


;--------------------------------------    
;S1 DEBOUNCING
;------------------------------
check_s1:
    BIT.B   #0x01, &P4IN
    JNZ     s1_released
    MOV.W   #0, &s1_up_count
    CMP.W   #20, &s1_down_count
    JHS     s1_check_press        ;skips the increment if >=20 ,count
    INC.W   &s1_down_count

s1_check_press:
    CMP.W   #20, &s1_down_count
    JNE     check_s2
    CMP.W   #0, &s1_press
    JNE     check_s2
    MOV.W   #1, &s1_new_press
    MOV.W   #1, &s1_press
    JMP     check_s2

s1_released:
    MOV.W   #0, &s1_down_count
    CMP.W   #20, &s1_up_count
    JHS     s1_check_release
    INC.W   &s1_up_count

s1_check_release:
    CMP.W   #20, &s1_up_count
    JNE     check_s2
    MOV.W   #0, &s1_press   ;resetted, ready for next press  

;--------------------------------------    
    ;S2 DEBOUNCING
;------------------------------
check_s2:
    BIT.B   #0x02, &P4IN
    JNZ     s2_released
    MOV.W   #0, &s2_up_count
    CMP.W   #20, &s2_down_count
    JHS     s2_check_press        ;skips the increment if >=20 ,count
    INC.W   &s2_down_count

s2_check_press:
    CMP.W   #20, &s2_down_count
    JNE     press_complete
    CMP.W   #0, &s2_press
    JNE     press_complete
    MOV.W   #1, &s2_new_press
    MOV.W   #1, &s2_press
    JMP     press_complete

s2_released:
    MOV.W   #0, &s2_down_count
    CMP.W   #20, &s2_up_count
    JHS     s2_check_release
    INC.W   &s2_up_count

s2_check_release:
    CMP.W   #20, &s2_up_count
    JNE     press_complete
    MOV.W   #0, &s2_press   ;resetted, ready for next press      

;--------------------------------------------------
;presses are complete, EMERGENCY HAS PRIORITY
;--------------------------------------------------

press_complete:
    CMP.W   #1, &s2_new_press
    JNE     s1_complete
    CMP.W   #0, &emergency
    JNE     emergency_finish
    MOV.W   #1, &emergency
    MOV.W   #0, &emergency_count
    BIS.B   #0xF0, &P3OUT
    MOV.W   #msg_em_start, R12          ; UART: "EMERGENCY CALL START"
    CMP.W   #0, &normal                 ; UART: add "NORMAL CALL SUSPEND" if
    JEQ     uart_em_start               ; UART:   a normal call was running
    MOV.W   #msg_em_suspend, R12
uart_em_start:
    CALL    #uart_record                ; UART: send it
    JMP     timer_done

emergency_finish:
    MOV.W   #0, &emergency       ; 
    BIC.B   #0xF0, &P3OUT        ; Turn off LED5–8 coz emergency = 0
    CMP.W   #0, &normal
    JEQ     emergency_to_idle   
    MOV.W   #msg_em_resume, R12
    CALL    #uart_record
    JMP     timer_done

emergency_to_idle:
    MOV.W   #msg_em_idle, R12
    CALL    #uart_record
    JMP     timer_done

s1_complete:

    CMP.W   #1, &s1_new_press
    JNE     timer_done
    CMP.W   #0, &emergency          ; UART: S1 during an emergency?
    JEQ     uart_s1_ok
    MOV.W   #msg_s1_ignored, R12    ; UART: "S1 IGNORED: EMERGENCY CALL ACTIVE"
    CALL    #uart_record
uart_s1_ok:
    CMP.W   #0, &emergency
    JNE     timer_done
    CMP.W   #0, &normal
    JNE     normal_finished
    MOV.W   #1, &normal
    MOV.W   #0, &normal_count
    MOV.W   #0x01, &normal_led
    BIC.W   #0x0F, &PJOUT
    BIS.W   &normal_led, &PJOUT
    MOV.W   #msg_normal_start, R12  ; UART: "NORMAL CALL START; LED = 1"
    CALL    #uart_record
    JMP     timer_done

normal_finished:
    MOV.W   #0, &normal
    BIC.W   #0x0F, &PJOUT
    MOV.W   #msg_normal_done, R12   ; UART: "NORMAL CALL COMPLETE"
    CALL    #uart_record

    
timer_done:    
    POP.W   R15                     ; UART: restore saved registers
    POP.W   R14
    POP.W   R13
    POP.W   R12
    RETI                            ; retursn the interrupted code
;===============================================================================
; UART CODE
;===============================================================================

;-------------------------------------------------------------------------------
; UART: clock. DCO = 8 MHz. SMCLK = 8/8 = 1 MHz timer and 9600 baud.
; MCLK (CPU) = 8 MHz 
;-------------------------------------------------------------------------------
init_clock:
    MOV.B   #CSKEY_H, &CSCTL0_H         ; unlock clock registers
    MOV.W   #DCOFSEL_3, &CSCTL1         ; DCO = 8 MHz
    BIC.W   #SELS_7, &CSCTL2            ; SMCLK source = DCO
    BIS.W   #SELS__DCOCLK, &CSCTL2
    BIC.W   #DIVS0|DIVS1|DIVS2, &CSCTL3 ; SMCLK = DCO / 8 = 1 MHz
    BIS.W   #DIVS__8, &CSCTL3
    BIC.W   #DIVM0|DIVM1|DIVM2, &CSCTL3 ; MCLK  = DCO / 1 = 8 MHz
    MOV.B   #0, &CSCTL0_H               ; lock clock registers
    RET

;-------------------------------------------------------------------------------
; UART: set-up. eUSCI_A0, P2.0 = TX, P2.1 = RX, 9600 baud, 8N1
;-------------------------------------------------------------------------------
init_uart:
    BIS.B   #BIT0|BIT1, &P2SEL1         ; P2.0 / P2.1 = UART pins
    BIC.B   #BIT0|BIT1, &P2SEL0
    MOV.W   #UCSWRST|UCSSEL__SMCLK, &UCA0CTLW0 ; hold in reset, clock = SMCLK
    MOV.W   #6, &UCA0BRW                ; 9600 baud at 1 MHz
    MOV.W   #0x2081, &UCA0MCTLW
    MOV.W   #0, &tx_head                ; buffer empty
    MOV.W   #0, &tx_tail
    MOV.W   #0, &tx_active
    BIC.W   #UCSWRST, &UCA0CTLW0        ; release from reset
    RET

;-------------------------------------------------------------------------------
; UART: uart_put - put the character in R13 into the transmit buffer.
; Called with interrupts off 
; If the buffer is full the character is dropped.
;-------------------------------------------------------------------------------
uart_put:
    PUSH.W  R14                         ; save registers used here
    PUSH.W  R15
    MOV.W   &tx_head, R14               ; next position = head + 1
    MOV.W   R14, R15
    INC.W   R15
    CMP.W   #TX_SIZE, R15               ; wrap to 0 at the end
    JLO     up_no_wrap
    MOV.W   #0, R15
up_no_wrap:
    CMP.W   &tx_tail, R15               ; buffer full?
    JEQ     up_done                     ;   yes: drop the character
    MOV.B   R13, tx_buffer(R14)         ; store character
    MOV.W   R15, &tx_head
    CMP.W   #0, &tx_active              ; UART idle?
    JNE     up_done
    MOV.W   #1, &tx_active              ;   yes: start sending
    BIS.W   #UCTXIE, &UCA0IE            ;   TX interrupt sends the buffer
up_done:
    POP.W   R15
    POP.W   R14
    RET

;-------------------------------------------------------------------------------
; UART: uart_record - send "[nnnnnn ms] <text>" + new line. R12 = text address.
; Each timestamp digit = how many times a power of ten can be subtracted
; At least 6 digits are always sent.
; R14:R15 = time in ms (low:high), R12 = table pointer, R13 = digit.
;-------------------------------------------------------------------------------
uart_record:
    PUSH.W  R12                         ; keep the text address
    MOV.W   #'[', R13
    CALL    #uart_put

    MOV.W   &ms_lo, R14                 ; time in ms (32-bit)
    MOV.W   &ms_hi, R15
    MOV.W   #0, &ur_started
    MOV.W   #pow10_table, R12           ; start at 1 000 000 000
ur_digit:
    MOV.W   #'0', R13                   ; digit starts at '0'
ur_sub:
    CMP.W   2(R12), R15                 ; while time >= power of ten
    JLO     ur_got
    JNE     ur_take
    CMP.W   0(R12), R14
    JLO     ur_got
ur_take:
    SUB.W   0(R12), R14                 ;   time = time - power (32-bit)
    SUBC.W  2(R12), R15
    INC.W   R13                         ;   digit + 1
    JMP     ur_sub
ur_got:
    CMP.W   #'0', R13                   ; skip leading zeros...
    JNE     ur_print
    CMP.W   #0, &ur_started
    JNE     ur_print
    CMP.W   #pow10_six, R12             ; ...but always send the last 6 digits
    JLO     ur_next
ur_print:
    MOV.W   #1, &ur_started
    CALL    #uart_put                   ; send the digit
ur_next:
    ADD.W   #4, R12                     ; next power of ten
    CMP.W   #pow10_end, R12
    JNE     ur_digit

    MOV.W   #str_ms, R12                ; " ms] "
    CALL    #uart_text
    POP.W   R12                         ; record text
    CALL    #uart_text
    MOV.W   #13, R13                    ; carriage return
    CALL    #uart_put
    MOV.W   #10, R13                    ; line feed
    CALL    #uart_put
    RET

;-------------------------------------------------------------------------------
; UART: uart_text - send the zero terminated text at R12.
;-------------------------------------------------------------------------------
uart_text:
    MOV.B   @R12+, R13                  ; next character
    CMP.B   #0, R13                     ; end of text?
    JEQ     ut_done
    CALL    #uart_put
    JMP     uart_text
ut_done:
    RET

;-------------------------------------------------------------------------------
; UART: uart_progress - "NORMAL CALL PROGRESS; LED = n" for the lit LED.
;-------------------------------------------------------------------------------
uart_progress:
    MOV.W   #msg_progress_1, R12
    CMP.W   #0x01, &normal_led
    JEQ     uprog_send
    MOV.W   #msg_progress_2, R12
    CMP.W   #0x02, &normal_led
    JEQ     uprog_send
    MOV.W   #msg_progress_3, R12
    CMP.W   #0x04, &normal_led
    JEQ     uprog_send
    MOV.W   #msg_progress_4, R12
uprog_send:
    CALL    #uart_record
    RET

;-------------------------------------------------------------------------------
; UART: TX interrupt - sends one character each time the UART is ready.
;-------------------------------------------------------------------------------
uart_isr:
    PUSH.W  R14
    PUSH.W  R15
    BIT.W   #UCTXIFG, &UCA0IFG          ; UART ready for the next character?
    JZ      uisr_done                   ; (flag check, as in Lecture 9)
    MOV.W   &tx_tail, R15
    CMP.W   &tx_head, R15               ; anything left to send?
    JEQ     uisr_empty
    MOV.B   tx_buffer(R15), R14         ; send next character
    MOV.W   R14, &UCA0TXBUF
    INC.W   R15                         ; move on, wrap at the end
    CMP.W   #TX_SIZE, R15
    JLO     uisr_save
    MOV.W   #0, R15
uisr_save:
    MOV.W   R15, &tx_tail
    JMP     uisr_done
uisr_empty:
    BIC.W   #UCTXIE, &UCA0IE            ; buffer empty: stop TX interrupts
                                        ; (ready flag stays set, so turning
                                        ;  UCTXIE on again restarts sending)
    MOV.W   #0, &tx_active
uisr_done:
    POP.W   R15
    POP.W   R14
    RETI

;-------------------------------------------------------------------------------
; UART: record text
;-------------------------------------------------------------------------------
msg_ready:        .string "SYSTEM READY; IDLE"
                  .byte   0
msg_normal_start: .string "S1 -> NORMAL CALL START; LED = 1"
                  .byte   0
msg_progress_1:   .string "NORMAL CALL PROGRESS; LED = 1"
                  .byte   0
msg_progress_2:   .string "NORMAL CALL PROGRESS; LED = 2"
                  .byte   0
msg_progress_3:   .string "NORMAL CALL PROGRESS; LED = 3"
                  .byte   0
msg_progress_4:   .string "NORMAL CALL PROGRESS; LED = 4"
                  .byte   0
msg_normal_done:  .string "S1 -> NORMAL CALL COMPLETE; ENTER IDLE"
                  .byte   0
msg_em_start:     .string "S2 -> EMERGENCY CALL START"
                  .byte   0
msg_em_suspend:   .string "S2 -> EMERGENCY CALL START; NORMAL CALL SUSPEND"
                  .byte   0
msg_s1_ignored:   .string "S1 IGNORED: EMERGENCY CALL ACTIVE"
                  .byte   0
str_ms:           .string " ms] "
                  .byte   0
msg_em_resume:    .string "S2 -> EMERGENCY CALL COMPLETE; NORMAL CALL RESUME"
                  .byte   0   
msg_em_idle:      .string "S2 -> EMERGENCY CALL COMPLETE; ENTER IDLE"
                  .byte   0
                  .align  2
pow10_table:                            ; UART: 32-bit powers of ten (low, high)
    .word   0xCA00, 0x3B9A              ; 1000000000
    .word   0xE100, 0x05F5              ;  100000000
    .word   0x9680, 0x0098              ;   10000000
    .word   0x4240, 0x000F              ;    1000000
pow10_six:
    .word   0x86A0, 0x0001              ;     100000
    .word   0x2710, 0x0000              ;  10000
    .word   0x03E8, 0x0000              ;   1000
    .word   0x0064, 0x0000              ;    100
    .word   0x000A, 0x0000              ;     10
    .word   0x0001, 0x0000              ;      1
pow10_end:

    .global __STACK_SIZE
    .sect   .stack
    .sect   TIMER0_A0_VECTOR 
    .short  timer_A0
    .sect   USCI_A0_VECTOR              ; UART: TX interrupt vector
    .short  uart_isr
    .sect   ".reset"
    .short  RESET


