#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "iopin.h"

class pulseBuffer {
  public:
	enum : unsigned char {
		kSize =  128,
		kNBuffers = 2
	};
  private:
	unsigned short lPulses[128];
	unsigned char lIndex;
  public:
	pulseBuffer(): lIndex(0) {
	};
	bool fAddPulse(unsigned short aTime) {
		lPulses[lIndex++] = aTime;
		if (lIndex >= kSize) {
			return (false);
		}
		return (true);
	};
	void fClear() {
		lIndex = 0;
	};
};

pulseBuffer gBuffer[pulseBuffer::kNBuffers];
volatile unsigned char gWriteBufferIndex;
volatile unsigned char gNextReadBufferIndex;

IOPin(C, 0) ACOmonitor(true);


ISR(TIMER1_CAPT_vect) { // an edge was detected and captured
	cli();
	auto interval = TCNT1;
	if (bit_is_set(ACSR, ACO)) {
		interval |= 0x8000;
		ACOmonitor.fSet(true);
	} else {
		ACOmonitor.fSet(false);
	}
	gBuffer[gWriteBufferIndex].fAddPulse(interval);
	sei();
}

ISR(TIMER1_COMPA_vect) { // an edge timeout happened
	cli();
	if (1 /* pulse train found */) {
		gNextReadBufferIndex = gWriteBufferIndex;
		gWriteBufferIndex = (gWriteBufferIndex + 1) % pulseBuffer::kNBuffers;
	} else { /* maybe junk in the buffer, clear it */
		gBuffer[gWriteBufferIndex].fClear();
	}
	sei();
}
int main(void) {
	gNextReadBufferIndex = 0xFFu; // mark next read buffer as invalid


	// enable comparator with builtin reference
	ACSR = _BV(ACBG); // ACIE
	TCCR1A = 0; // normal mode
	TCCR1B = ICNC1 // noise canceller on
	         | ICES1 // capture on positive edge
	         | WGM12 // use CTC mode with TOP at OCR1A
	         | CS11 | CS10; // use sysclck/64 as counting freq, i.e. 250hKz or 4us ticks.

	DDRB = 1; // set portb bit0 aus output, use for monitor of comp out
	TIMSK = TICIE1 // enable capture interrupt
	        | OCIE1A; // enable compare interruppt
	while (true) {
		if (gNextReadBufferIndex == 0xFF) { // no valid next read buffer
			continue;
		}
		cli(); // atomic operation to get the gNextReadBufferIndex read and set to invalid
		auto readBufferIndex = gNextReadBufferIndex;
		gNextReadBufferIndex = 0xFF;
		sei();
		gBuffer[readBufferIndex].fClear(); // clear buffer after reading
	}
}
