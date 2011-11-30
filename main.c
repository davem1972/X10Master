/*
 * (C) Copyright 2011, Dave McCaldon <davem@mccaldon.com>
 * All Rights Reserved.
 *
 * X10Master implements an X10 interface via i2c to a X10 Powerline
 * interface device (PSC05) via the TW523 protocol.
 * 
 */


#include <avr/io.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <string.h>
#include <util/delay.h>

#include "usiTwiSlave.h"
#include "ringbuffer.h"
#include "logevents.h"
#include "commands.h"


#define X10_MASTER_I2C_ADDRESS    0x28
#define X10_MASTER_LOG_BUFFERSIZE 32

#define X10_MASTER_SR_LOGOVERFLOW 0x01
#define X10_MASTER_SR_X10ERROR    0x02

#define X10_DELAY_OFFSET          500
#define X10_DELAY_HALF_CYCLE      8334

#define BYTES(...) (uint8_t[]){ __VA_ARGS__ }

#define STATUS_PORT_PIN           PA7
#define STATUS_PORT_PORT          PORTA
#define STATUS_PORT_DDR           DDRA

#define X10_PIN_ZC                PB6
#define X10_PIN_RX                PB5
#define X10_PIN_TX                PB4
#define X10_PORT_IN               PINB
#define X10_PORT_OUT              PORTB
#define X10_PORT_DDR              DDRB

/**
 * Macro to induce a short delay (2 clock cycles)
 */
#define shortdelay()	\
	__asm__(			\
		"nop\n\t" 		\
		"nop\n\t"  		\
		);

/*
 * Global state
 */
volatile uint16_t       status           = 0;
volatile uint8_t        status_register  = 0;
volatile uint32_t       uptime           = 0;
uint8_t                 log_buffer_data[X10_MASTER_LOG_BUFFERSIZE];
RB_RINGBUFFER           log_buffer;

/*
 * X10 State
 */
volatile uint8_t x10_sendmode  = 0;
volatile uint8_t x10_housecode = 0;
volatile uint8_t x10_unitcode  = 0;
volatile uint8_t x10_cmdcode   = 0;

volatile uint16_t x10_bitcount = 0;
volatile uint16_t x10_zccount  = 0;
volatile uint16_t x10_recvbuff = 0;
volatile uint16_t x10_mask     = 0;

/**
 * X10 House Codes lookup table
 */
char const X10_HOUSE_CODES[16] PROGMEM = {
	'M',	// 0000
	'E',    // 0001
	'C',    // 0010
	'K',    // 0011
	'O',    // 0100
	'G',    // 0101
	'A',    // 0110
	'I',    // 0111
	'N',    // 1000
	'F',    // 1001
	'D',    // 1010
	'L',    // 1011
	'P',    // 1100
	'H',    // 1101
	'B',    // 1110
	'J'     // 1111
};
  
/**
 * X10 Unit Codes lookup table
 */
uint8_t const X10_UNIT_CODES[16] PROGMEM = {
	13,     // 0000
	 5,     // 0001
	 3,     // 0010
    11,     // 0011
	15,     // 0100
	 7,     // 0101
	 1,     // 0110
     9,     // 0111
	14,     // 1000
	 6,     // 1001
	 4,     // 1010
	12,     // 1011
	16,     // 1100
	 8,     // 1101
	 2,     // 1110
	10      // 1111
};


/**
 * Initialize the log
 */
void loginit()
{
    RB_Initialize(&log_buffer,
    			  log_buffer_data,
    			  X10_MASTER_LOG_BUFFERSIZE);

	memset((void*)log_buffer_data,
		   '@',
		   X10_MASTER_LOG_BUFFERSIZE);
}

/**
 * Log an event to the log buffer
 */
size_t logevent(uint8_t* event, size_t eventlen)
{
    size_t inserted = RB_Insert(&log_buffer, event, eventlen);

	if (inserted < eventlen) {
		// Log has overflowed, we need to reset it
		cli();

		// We must reset the log, and remember that it overflowed
		status_register |= X10_MASTER_SR_LOGOVERFLOW;
		loginit();

		// Reenable interrupts
		sei();
	}

	return inserted;
}

/**
 * Callback used to transmit the contents of the log buffer to the i2c master.
 */
size_t twi_rb_transmit_cb(RB_RINGBUFFER* rb,
						  uint8_t*       data,
						  size_t         count,
						  void**         token)
{
    size_t ptr = 0;

    while (ptr < count) {
        size_t len = count - ptr;

		if (len > X10_MASTER_LOG_BUFFERSIZE) len = X10_MASTER_LOG_BUFFERSIZE;

        usiTwiTransmitByte((uint8_t)len);

        while (len > 0) {
            usiTwiTransmitByte(*data);
            data++;
            len--;
            ptr++;
        }
    }

    return ptr;
}

/**
 * Transmit the contents of the log buffer to the i2c master
 */
size_t transmit_log()
{
    size_t count = RB_ReadWithCallback(&log_buffer,
    								   log_buffer.size,
    								   twi_rb_transmit_cb, 0);

	// Send a zero to mark the end of the log
	usiTwiTransmitByte(0);

	// Clear the LOGOVERFLOW flag (if set)
	status_register &= ~X10_MASTER_SR_LOGOVERFLOW;

	return count;
}

/**
 * Initialize
 */
void init()
{
/*
	MCUSR  &= 0xF7;		// Clear WDRF Flag
	WDTCSR	= 0x18;		// Set bits so we can clear watchdog timer...
	WDTCSR	= 0x00;		// Clear watchdog timer	
*/

    // Reset all DDRs
    DDRA = 0;
    DDRB = 0;

    // Clear all ports
    PORTA = 0;
    PORTB = 0;

	// Enable pullups for PB6 and PB5
	X10_PORT_DDR &= ~(_BV(X10_PIN_ZC) | _BV(X10_PIN_RX));
	X10_PORT_OUT |= _BV(X10_PIN_ZC) | _BV(X10_PIN_RX);

	// Now, set up PB6/INT0 for the X10 zero crossing
  	//PCMSK |= _BV(X10_PIN_ZC);

  	// Interrupt on any change on INT0 pin
	MCUCR &= ~_BV(ISC01);	// ISC01 = 0
	MCUCR |= _BV(ISC00);	// ISC00 = 1

  	// Turn on interrupt INT0
  	GIMSK |= _BV(INT0);

	// Data direction register: DDRD, set PORTD to output
	STATUS_PORT_DDR |= _BV(STATUS_PORT_PIN);		// PD6

	// Clear all output
	STATUS_PORT_PORT &= ~_BV(STATUS_PORT_PIN);
    status = 0;

    // Set up the timer
    TCCR1A = 0x00;          // Timer counter control register 
    TCCR1B = _BV(CS12);     // Timer at F_CPU/8
    TIMSK |= _BV(TOIE1);	// Set bit 1 in TIMSK to enable Timer 1 overflow interrupt.
}

/**
 * Pulse the status light
 */
void statuspulse(uint16_t duration)
{
	// Turn on the status light
	status = duration;
}

/**
 * X10 Send Code
 */
int x10_send(uint8_t cmd, uint8_t hc, uint8_t uc)
{
    uint8_t result = 0;

    // First, wait until the previous send has completed
    while (x10_sendmode) { shortdelay(); }

    // Convert the house code into it's binary form
    for (x10_housecode = 0; x10_housecode < 16; x10_housecode++) {
        if (X10_HOUSE_CODES[x10_housecode] == hc) break;
    }

    // And the unit code
    for (x10_unitcode = 0; x10_unitcode < 16; x10_unitcode++) {
        if (X10_UNIT_CODES[x10_unitcode] == uc) break;
    }

    // The command is as is
    x10_cmdcode = cmd;

    // Validate before pushing the frame out
    if ((x10_housecode < 16) && (x10_unitcode < 16) && (x10_cmdcode < 16)) {

        // Now, let's wait until the send has completed.
        x10_sendmode = 1;
        
        // The zero crossing ISR (INT0) will process the X10 frame onto the TX
        // line of the PSC05 via the TW523 protocol (see do_x10_send()).
        while (x10_sendmode) { shortdelay(); }

    } else {
        // Invalid house, unit or command code
        x10_housecode = 0;
        x10_unitcode  = 0;
        x10_cmdcode   = 0;

        // Failed ...
        result = 1;
    }


    return result;
}

/**
 * Ping via the i2c bus.  Responds with 'PONG'
 */
void i2c_ping()
{
    logevent(BYTES(X10_MASTER_EVENT_PING), 1);
    usiTwiTransmitByte('P');
    usiTwiTransmitByte('O');
    usiTwiTransmitByte('N');
    usiTwiTransmitByte('G');
}

/**
 * Read the ATtiny861 uptime
 */
void i2c_uptime()
{
    logevent(BYTES(X10_MASTER_EVENT_UPTIME), 1);
    usiTwiTransmitByte(uptime & 0xFF);
    usiTwiTransmitByte((uptime >> 8) & 0xFF);
    usiTwiTransmitByte((uptime >> 16) & 0xFF);
    usiTwiTransmitByte((uptime >> 24) & 0xFF);
}

/**
 * Read the status register
 */
void i2c_status()
{
	usiTwiTransmitByte(status_register);
}

/**
 * Read the internal log
 */
void i2c_readlog()
{
	transmit_log();
}

/**
 * Send an X10 code
 */
void i2c_sendcode()
{
    uint8_t cmd = usiTwiReceiveByte();
    uint8_t hc  = usiTwiReceiveByte();
    uint8_t uc  = usiTwiReceiveByte();

    logevent(BYTES(X10_MASTER_EVENT_X10_SEND_CODE, cmd, hc, uc), 4);

    uint8_t rc  = x10_send(cmd, hc, uc);

    usiTwiTransmitByte(rc);
}

/**
 * Report a bad i2c command
 */
void i2c_badcmd(uint8_t command)
{
    // Send back a -1 to indicate error
    usiTwiTransmitByte(0xFF);

    // Report an invalid command in the log
    logevent(BYTES(X10_MASTER_EVENT_INVALID_COMMAND, command), 2);
}

/**
 * Main entry point.  Note that this never returns, when you get to the end
 * it simply loops forever.
 *
 * Since we use interrupts to drive the i2c slave, this loop simply checks
 * for a X10 command to execute and, if one is available will execute
 * it.  Otherwise, it goes to sleep to save power; the CPU will wake up
 * on receipt of i2c traffic.
 *
 * Upon receipt of a valid X10 command and arguments, the command is executed
 * and the results are sent back to the i2c master.
 */
int main(void)
{
    init();

    loginit();
    logevent(BYTES(X10_MASTER_EVENT_STARTUP), 1);

    // Enable IDLE sleep mode
//    set_sleep_mode(SLEEP_MODE_IDLE);

    // Set up this slave at the specified address
    usiTwiSlaveInit(X10_MASTER_I2C_ADDRESS);

    // Enable interrupts
    sei();

    // Long pulse on the status light
    statuspulse(1000);

    // Loop forever, wait for an X10 command to execute
    for (;;) {

        // Disable interrupts while we check for work ...
        //cli();

        // Check for an inbound i2c command ...
        if (usiTwiDataInReceiveBuffer()) {

            // Renable interrupts, let's process the request
            //sei();

            // Blink the status light to show we got a command
            statuspulse(100);

            // Read the command byte
            uint8_t command = usiTwiReceiveByte();

            // Dispatch the i2c command
            switch (command) {
            	case X10_MASTER_COMMAND_PING:			i2c_ping(); 	break;
            	case X10_MASTER_COMMAND_UPTIME:			i2c_uptime();	break;
            	case X10_MASTER_COMMAND_STATUS:			i2c_status();	break;
            	case X10_MASTER_COMMAND_READLOG:		i2c_readlog();	break;
            	case X10_MASTER_COMMAND_X10_SENDCODE:	i2c_sendcode();	break;

            	default:
            		// Bad command!
            		i2c_badcmd(command);
            		break;
            }
        } else {

            // No inbound command, let's sleep
#if defined(SLEEP_ON_IDLE)
            sleep_enable();
#if defined(sleep_bod_disable)
            sleep_bod_disable();
#endif
            // Re-enable interrupts
            sei();
            sleep_cpu();
            sleep_disable();
#else
            // Reenable interrupts
            //sei();
#endif
        }
    }
}

/**
 * X10 Send
 */
void do_x10_send()
{
}

/**
 * X10 Receive
 */
void do_x10_recv()
{
	// Check for start of frame
	if (x10_bitcount == 0) {
		_delay_us(X10_DELAY_OFFSET);

		// Check for start bit, otherwise give up
		if (X10_PORT_IN & _BV(X10_PIN_RX)) return;

		status        = 65535;

		x10_recvbuff  = 0x1000;
		x10_mask      = (x10_recvbuff >> 1);

		x10_bitcount  = 1;
		x10_zccount   = 1;

	} else {

		// Mid frame
		x10_zccount++;

		// Grab bits, first the 4 start bits, then every odd bit (to ignore parity bits)
		if ((x10_bitcount < 5) || (x10_zccount & 1)) {
			_delay_us(X10_DELAY_OFFSET);

			if ((X10_PORT_IN & _BV(X10_PIN_RX)) == 0) {
				// Got a 1, otherwise it's zero
				x10_recvbuff |= x10_mask;
			}

			x10_mask >>= 1;
			x10_bitcount++;

			// Check for the end of the frame
			if (x10_bitcount == 13) {

				status = 0;

				// Reset for the next frame
				x10_bitcount = 0;
				x10_zccount  = 0;

				// Delay a bit
				_delay_us(X10_DELAY_HALF_CYCLE);
				_delay_us(X10_DELAY_HALF_CYCLE);
				_delay_us(X10_DELAY_HALF_CYCLE);
				_delay_us(X10_DELAY_HALF_CYCLE);
				_delay_us(X10_DELAY_HALF_CYCLE);

				// Now, parse the X10 frame and log it

				// Find the house code
				x10_housecode = (x10_recvbuff >> 5) & 0xF;

				// Check for a command code vs a unit code
				if (x10_recvbuff & 0x1) {
					// If the last bit is set, then it's a command
					x10_cmdcode = x10_recvbuff & 0x1F;

					// Look up the house, unit and command codes
					char    hc = pgm_read_byte(&X10_HOUSE_CODES[x10_housecode & 0xF]);
					uint8_t uc = pgm_read_byte(&X10_UNIT_CODES[(x10_unitcode >> 1) & 0xF]);
					char    cc = x10_cmdcode;

					// Now, we can log this!
					logevent(BYTES(X10_MASTER_EVENT_X10_RECV_CODE, cc, hc, uc), 4);

					// Reset these ...
					x10_cmdcode   = 0;
					x10_housecode = 0;
					x10_unitcode  = 0;

				} else {
					// Unit code
					x10_unitcode = x10_recvbuff & 0x1F;
				}
			}
		}
	}
}

/* ***************************************************************************
 *                         Interrupt Service Routines
 * ************************************************************************ */

/**
 * INT0 interrupt (X10 zero crossing)
 */
ISR(INT0_vect)
{
    if (x10_sendmode) {
        do_x10_send();
    } else {
        do_x10_recv();
    }
}

/**
 * Status interrupt
 */
ISR(TIMER1_OVF_vect)
{
    if (status) {
		if (uptime & 7) {
	        // Status light should be on ...
	        STATUS_PORT_PORT |= _BV(STATUS_PORT_PIN);
		    shortdelay();
		    shortdelay();
		    shortdelay();
		    shortdelay();
		    shortdelay();
		    shortdelay();
		    shortdelay();
		    shortdelay();
	        STATUS_PORT_PORT &= ~_BV(STATUS_PORT_PIN);

			status--;
		}
    } else {
	    // Turn it off
	    STATUS_PORT_PORT &= ~_BV(STATUS_PORT_PIN);
	}

    // Bump our uptime counter
    uptime++;
} 


/*
 * End-of-file
 *
 */
