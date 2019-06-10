; ------------------------------------------------------------------------------
; - YM2149 Streamer for ATmega328P - Improved Version                          -
; -                                                                            -
; - This assembly version is written by Randy Thiemann and based on the        -
; - excellent C version by Florent Flament.                                    -
; -                                                                            -
; - Pinout:                                                                    -
; -   PORTD5, PORTB3, PORTD3: LEDs 1, 2 and 3.                                 -
; -   PORTC0-PORTC5: DATA0-DATA5                                               -
; -   PORTD6-PORTD7: DATA6-DATA7                                               -
; -   PORTB4, PORTB5: BC1, BDIR                                                -
; -   PORTD0, PORTD1: RXD, TXD                                                 -
; -   PORTB0-PORTB2, PORTD2, PORTD4, ADC6, ADC7: Available but clobbered.      -
; -                                                                            -
; - Usage:                                                                     -
; -   Device will reboot upon connection of a serial connection, at which point-
; -   it will transmit the amount of free bytes in the buffer. It will store   -
; -   exactly that many bytes in the buffer before again transmitting the      -
; -   amount of free room in the buffer. This process continues indefinitely.  -
; -   The baudrate is configurable below, format 8-N-1.                        -
; -                                                                            -
; -   Song data is expected in packets in the following format:                -
; -   2 bytes: Time in clock cycles for the YM2149 for the sample to be played.-
; -            since the last sample. High byte first.                         -
; -   2*n bytes: A series of byte pairs, the first being the register number,  -
; -              the second being the register value. N may be 0 to allow for  -
; -              filler packets.                                               -
; -   1 byte: 0xFF to signify the end of packet. 0xFE to signify end of song.  -
; ------------------------------------------------------------------------------

    ; Options: 250000, 500000, or 1000000
    .equ    BITRATE         = 500000

    ; If set to 0, the playback routine will ignore the YM2149's specsheet and
    ; toggle as fast as it can. This may not work on all chips.
    .equ    RESPECT_SPECS   = 1

    .nolist
    ; ATmega328P definitions, but we use our own names for the X, Y, and Z regs.
    .include "m328pdef.inc"
    .undef  XL
    .undef  XH
    .undef  YL
    .undef  YH
    .undef  ZL
    .undef  ZH
    .listmac
    .list

    ; Constants
    .equ    BC1_PIN         = 5
    .equ    BDIR_PIN        = 4
    .equ    WDAT_MODE       = 0x20
    .equ    ADDR_MODE       = 0x30
    ; We use every single byte of RAM.
    ; Total memory = 2048b, 2b for the stack, just enough for an interrupt
    .equ    BUF_SIZE        = 2046 
    .equ    RX_XMIT         = 0
    .equ    RX_RCV          = 1
    .equ    SMP_SAMPLEGET   = 0
    .equ    SMP_SAMPLEPROC  = 1

    ; Register assignments
    .def    temp_sreg    = r0
    .def    chunk_size_l = r2
    .def    chunk_size_h = r3
    .def    count_l      = r4
    .def    count_h      = r5
    .def    buf_end_l    = r6
    .def    buf_end_h    = r7
    .def    free_cnt_l   = r8
    .def    free_cnt_h   = r9
    .def    xmit_get     = r10
    .def    mode_0       = r12
    .def    mode_1       = r13
    .def    mode_2       = r14
    .def    temp_1       = r16
    .def    temp_2       = r17
    .def    can_start    = r18
    .def    isr_temp     = r19
    .def    addr         = r20
    .def    val          = r21
    .def    rx_state     = r22
    .def    smp_state    = r23
    .def    cnt_16_l     = r24
    .def    cnt_16_h     = r25
    .def    first_l      = r26
    .def    first_h      = r27
    .def    last_l       = r28
    .def    last_h       = r29
    .def    progmem_l    = r30
    .def    progmem_h    = r31

.macro vol
    mov     progmem_l, val
    lpm     val, Z
.endmacro

.macro bufg
    ld      @0, X+
    cp      first_l, buf_end_l
    cpc     first_h, buf_end_h
    brne    PC+3
    ldi     first_l, low(buf)
    ldi     first_h, high(buf)
.endmacro

; Interrupt vector table.
.cseg
    .org    0x0000
    rjmp    reset
    .org    URXCaddr
    rjmp    isr_usart_rx
    .org    INT_VECTORS_SIZE

; ---- RESET VECTOR ---- ;
reset:
    ; Stack pointer initialization.
    ldi     temp_1, low(RAMEND)
    out     SPL, temp_1
    ldi     temp_1, high(RAMEND)
    out     SPH, temp_1
    ; Set up buffer pointers.
    ldi     last_h, high(buf)
    ldi     last_l, low(buf)
    movw    first_h:first_l, last_h:last_l
    ldi     temp_1, high(buf+BUF_SIZE)
    mov     buf_end_h, temp_1
    ldi     temp_1, low(buf+BUF_SIZE)
    mov     buf_end_l, temp_1
    ; Set up serial device.
    clr     temp_1
    sts     UBRR0H, temp_1
.if BITRATE == 500000
    ldi     temp_1, 1
.elif BITRATE == 250000
    ldi     temp_1, 3
.else ; BITRATE == 1000000
.endif
    sts     UBRR0L, temp_1
    ldi     temp_1, (1<<TXEN0)|(1<<RXEN0)|(1<<RXCIE0)
    sts     UCSR0B, temp_1
    ldi     temp_1, (1<<UCSZ00)|(1<<UCSZ01)
    sts     UCSR0C, temp_1
    ; Set up timers.
    in      temp_1, TCCR0A
    ori     temp_1, (1<<WGM00)|(1<<WGM01)|(1<<COM0B1)
    out     TCCR0A, temp_1
    in      temp_1, TCCR0B
    ori     temp_1, (1<<CS00)
    out     TCCR0B, temp_1
    lds     temp_1, TCCR1B
    ori     temp_1, (1<<CS11)
    sts     TCCR1B, temp_1
    lds     temp_1, TCCR2A
    ori     temp_1, (1<<COM2A1)|(1<<COM2B1)|(1<<WGM20)|(1<<WGM21)
    sts     TCCR2A, temp_1
    lds     temp_1, TCCR2B
    ori     temp_1, (1<<CS20)
    sts     TCCR2B, temp_1
    ; Setup GPIO
    ldi     temp_1, 0x3F ; PB0-PB5
    out     DDRB, temp_1
    ldi     temp_1, 0x3F ; PC0-PC5
    out     DDRC, temp_1
    ldi     temp_1, 0xFE ; PD1-PD7
    out     DDRD, temp_1
    ldi     temp_1, RX_XMIT
    mov     xmit_get, temp_1
    ldi     rx_state, RX_XMIT
    ldi     smp_state, SMP_SAMPLEGET
    ldi     progmem_h, high(lut<<1)
    clr     can_start
    clr     mode_0
    ldi     temp_1, ADDR_MODE
    mov     mode_1, temp_1
    ldi     temp_1, WDAT_MODE
    mov     mode_2, temp_1

    ; Clear the registers.
    rcall   clear_registers

    ; Enable interrupts and jump to the RX state machine since the
    ; buffer will be empty.
    sei
    rjmp    rx_state_machine

freeze:
    rcall   clear_registers
    rjmp    PC

    ; Sample state machine
smp_state_machine:
    cpse    smp_state, xmit_get
    rjmp    smp_proc_handler

smp_get_handler:
    ; --- Handle case SMP_SAMPLEGET ---
    ; Get the data we need, we assume that the buffer can keep up.
    bufg    temp_1
    bufg    temp_2
    ; Adjust the timer
    sts     OCR1AH, temp_1
    sts     OCR1AL, temp_2
    clr     temp_1
    sts     TCNT1H, temp_1
    sts     TCNT1L, temp_1
    sbi     TIFR1, OCF1A
    ldi     smp_state, SMP_SAMPLEPROC

smp_proc_handler:
    ; --- Handle case SMP_SAMPLEPROC ---
    ; Wait for the appropriate time
    in      temp_1, PORTB
    sbis    TIFR1, OCF1A
    rjmp    rx_state_machine
smp_proc_handler_get_addr:
    ; Disable interrupts in critical section
.if BITRATE != 1000000
    cli
.endif
    ; Get the address
    bufg    addr
    ; Check for special values
    cpi     addr, 0xFE
    ; If address below 0XFE, continue.
    brlo    smp_proc_handler_get_val
    ; If address = 0xFE, enter an infinite loop.
    breq    freeze
    ; Otherwise, return to the GET state and re-enable the interrupts.
.if BITRATE != 1000000
    sei
.endif
    ldi     smp_state, SMP_SAMPLEGET
    rjmp    smp_get_handler
smp_proc_handler_get_val:
    ; Get the value
    bufg    val
    
    ; ---- TIMING CRITICAL SECTION! ----
    ; NOTE: Depending on your specific chip, you may be able to comment out
    ; any or all of the nop instructions below.
    ;
    ; One clock cycle equals 65ns.
    ; At this point we know we have the address and the value loaded.
    ; We need to put the YM2149 in address mode before putting the data
    ; on the data lines.
    out     PORTB, mode_1
    ; We write the address to PORTC and PORTD. Because all the pins of
    ; PORTD that we aren't allowed to change are controlled by periphs,
    ; we can just write to it and not care!
    out     PORTD, addr
    out     PORTC, addr
    ; tAS = 300ns, meaning that the data needs to remain for 5 cycles.
    ; After this time is up, we turn off the address mode.
.if RESPECT_SPECS == 1
    nop
    nop
    nop
    nop
.endif
    out     PORTB, mode_0
    ; tAH = 80ns, or 2 cycles. The data needs to remain for this time
    ; after switching out of address mode. No delays are needed here,
    ; because the setup for write mode takes longer. We first write
    ; the data onto the data lines.
.if RESPECT_SPECS == 1
    nop
.endif
    out     PORTD, val
    out     PORTC, val
    ; tDS = 0ns, so we may switch immediately to data write mode.
    out     PORTB, mode_2
    ; tDW = 300ns, data must remain on the bus for 5 cycles before
    ; clearing the mode.
.if RESPECT_SPECS == 1
    nop
    nop
    nop
    nop
.endif
    out     PORTB, mode_0
    ; tDH = 80ns, but the data will remain on the bus for an entire
    ; loop.
    ; ---- END TIMING CRITICAL SECTION! ----

    ; Check if we need to update the lightshow and do so.
    cpi     addr, 0x08
    brne    smp_val_handler_lights_2
    vol
    out     OCR0B, val
    rjmp    smp_val_handler_post_lights
smp_val_handler_lights_2:
    cpi     addr, 0x09
    brne    smp_val_handler_lights_3
    vol
    sts     OCR2A, val
    rjmp    smp_val_handler_post_lights
smp_val_handler_lights_3:
    cpi     addr, 0x0A
    brne    smp_val_handler_post_lights
    vol
    sts     OCR2B, val
smp_val_handler_post_lights:
    ; Get the next address.
    rjmp    smp_proc_handler_get_addr

    ; If we should transmit...
rx_state_machine:
    cpse    rx_state, xmit_get
    rjmp    smp_state_machine
    ; Skip if TX buffer isn't empty...
    lds     temp_1, UCSR0A
    bst     temp_1, UDRE0
    brtc    rx_complete_handler_break
    cli
    cp      last_l, first_l
    cpc     last_h, first_h
    sei
    brsh    rx_complete_handler_else
    ; Calculate free_cnt...
    ; If first > last
    ; free_cnt = first - last - 1
    movw    cnt_16_h:cnt_16_l, first_h:first_l
    sub     cnt_16_l, last_l
    sbc     cnt_16_h, last_h
    sbiw    cnt_16_h:cnt_16_l, 1
    movw    free_cnt_h:free_cnt_l, cnt_16_h:cnt_16_l
    rjmp    rx_complete_after_free_cnt
rx_complete_handler_else:
    ; Else
    ; free_cnt = BUF_SIZE - 1 - (last - first)
    movw    cnt_16_h:cnt_16_l, last_h:last_l
    sub     cnt_16_l, first_l
    sbc     cnt_16_h, first_h
    ldi     temp_1, low(BUF_SIZE-1)
    ldi     temp_2, high(BUF_SIZE-1)
    sub     temp_1, cnt_16_l
    sbc     temp_2, cnt_16_h
    movw    free_cnt_h:free_cnt_l, temp_2:temp_1
rx_complete_after_free_cnt:
    ; If free_cnt == 0, break
    clr     temp_1
    cp      free_cnt_l, temp_1
    cpc     free_cnt_h, temp_1
    breq    rx_complete_handler_break
    ; Output free_cnt
    sts     UDR0, free_cnt_h
    sts     UDR0, free_cnt_l
    clr     count_l
    clr     count_h
    ; Turn over control to the ISR.
    ldi     rx_state, RX_RCV
rx_complete_handler_break:
    ; During the initial buffer, spin here until it is full.
    sbrs    can_start, 0
    rjmp    rx_complete_handler_break
    rjmp    smp_state_machine
; ---- RESET VECTOR ---- ;
    
; ---- USART RX VECTOR ---- ;
isr_usart_rx:
    in      temp_sreg, SREG
    ; Read a byte
    lds     isr_temp, UDR0
    st      Y+, isr_temp
    ; If at the end of the buffer, reset to start.
    cp      last_l, buf_end_l
    cpc     last_h, buf_end_h
    brne    isr_usart_rx_skip_3
    ldi     last_l, low(buf)
    ldi     last_h, high(buf)
isr_usart_rx_skip_3:
    ; Increment count.
    inc     count_l
    brne    isr_usart_rx_skip_4
    inc     count_h
isr_usart_rx_skip_4:
    ; Check if done.
    cp      free_cnt_l, count_l
    cpc     free_cnt_h, count_h
    brne    isr_usart_rx_4
    ori     can_start, 0xFF
    ldi     rx_state, RX_XMIT
isr_usart_rx_4:
    out     SREG, temp_sreg
    reti
; ---- USART RX VECTOR ---- ;

; Reset the registers
clear_registers:
    ldi     temp_2, 16
    clr     temp_1
clear_registers_loop:
    dec     temp_2
    out     PORTB, mode_1
    out     PORTD, temp_2
    out     PORTC, temp_2
    nop
    nop
    nop
    nop
    out     PORTB, mode_0
    nop
    out     PORTD, temp_1
    out     PORTC, temp_1
    out     PORTB, mode_2
    nop
    nop
    nop
    nop
    out     PORTB, mode_0
    brne    clear_registers_loop
    ret

; Program variables
.org 0x200
lut:
    .db 0, 1, 2, 3, 5, 7, 10, 15, 22, 31, 44, 63, 90, 127, 180, 255
    .db 0, 1, 2, 3, 5, 7, 10, 15, 22, 31, 44, 63, 90, 127, 180, 255

.dseg
.org 0x100
buf:
    .byte BUF_SIZE
stack_reserved:
    .byte 2
