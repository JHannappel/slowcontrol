#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "iopin.h"

class pulseBuffer {
  public:
	enum : unsigned char {
		kSize =  128,
		kNBuffers = 2,
		kMinEdges = 40
	};
	enum : unsigned short {
		kMinPulseLength = 5, // 100us
		kMaxCountValue = 0x7FFF
	};
  private:
	unsigned short lPulses[kSize];
	unsigned char lIndex;
  public:
	pulseBuffer(): lIndex(0) {
	};
	bool fAddPulse(unsigned short aTime, bool isHighPulse) {
		if (aTime < kMinPulseLength) {
			return false;
		}
		if (isHighPulse) {
			aTime += 0x8000u;
		}
		lPulses[lIndex++] = aTime;
		if (lIndex >= kSize) {
			return (false);
		}
		return (true);
	};
	void fClear() {
		lIndex = 0;
	};
	unsigned char fGetNEntries() {
		return lIndex;
	}
	unsigned short fGetEntry(unsigned char aIndex) {
		return lPulses[aIndex];
	}
};

pulseBuffer gBuffer[pulseBuffer::kNBuffers];
volatile unsigned char gWriteBufferIndex;
volatile unsigned char gNextReadBufferIndex;

IOPin(C, 0) ACOmonitor(true);
IOPin(C, 1) AquiringPulseTrain(true);
IOPin(C, 2) ProcessingPulseTrain(true);
IOPin(C, 3) TimeOut(true);
IOPin(C, 4) Capture(true);

ISR(ANA_COMP_vect) {
	ACOmonitor.fSet(bit_is_set(ACSR, ACO));
}

ISR(TIMER1_CAPT_vect) { // an edge was detected and captured
	cli();
	Capture.fSet(true);
	unsigned short interval = ICR1;
	//	bool wasPosEdge = bit_is_set(TCCR1B, ICES1);
	TCCR1B ^= _BV(ICES1); // change edge
	TIFR = _BV(ICF1); // reset interrupt flag
	TCNT1 = 0;
	if (gBuffer[gWriteBufferIndex].fAddPulse(interval, false) == false) { // probably an overflow
		gBuffer[gWriteBufferIndex].fClear();
		OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits
		TCCR1B |= _BV(ICES1); // wait for a positive edge
		AquiringPulseTrain.fSet(false);
	} else if (gBuffer[gWriteBufferIndex].fGetNEntries() == 2) {
		// set timeout for end of pulse train recognition to average of
		// the start and pause pulse at the biginning of the train
		OCR1A = ((gBuffer[gWriteBufferIndex].fGetEntry(0) & 0x7FFF) +
		         (gBuffer[gWriteBufferIndex].fGetEntry(1) & 0x7FFF)) >> 1;
	} else {
		AquiringPulseTrain.fSet(true);
	}
	Capture.fSet(false);
	sei();
}

ISR(TIMER1_COMPA_vect) { // an edge timeout happened
	cli();
	TimeOut.fSet(true);
	if (gBuffer[gWriteBufferIndex].fGetNEntries() > pulseBuffer::kMinEdges /* pulse train found */) {
		gNextReadBufferIndex = gWriteBufferIndex;
		gWriteBufferIndex = (gWriteBufferIndex + 1) % pulseBuffer::kNBuffers;
	} else { /* maybe junk in the buffer, clear it */
		gBuffer[gWriteBufferIndex].fClear();
	}
	OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits
	TCCR1B |= _BV(ICES1); // wait for a positive edge
	AquiringPulseTrain.fSet(false);
	TimeOut.fSet(false);
	sei();
}

void USART_Transmit( unsigned char data ) {
	/* Wait for empty transmit buffer */
	while ( !( UCSRA & (1 << UDRE)) );
	/* Put data into buffer, sends the data */
	UDR = data;
}

void USART_hexnibble(unsigned char nibble) {
	nibble &= 0x0f;
	if (nibble < 0x0A) {
		USART_Transmit('0' + nibble);
	} else {
		USART_Transmit('A' + nibble - 0x0A);
	}
}
void USART_hexbyte(unsigned char byte) {
	USART_hexnibble((byte >> 4) & 0x0f);
	USART_hexnibble(byte & 0x0f);
}
void USART_hexshort(unsigned short value) {
	USART_hexbyte((value >> 8) & 0xff);
	USART_hexbyte(value & 0xff);
}

int main(void) {
	gNextReadBufferIndex = 0xFFu; // mark next read buffer as invalid


	// enable comparator
	ACSR = _BV(ACIE) | _BV(ACIC);
	TCCR1A = 0; // normal mode
	TCCR1B = _BV(ICNC1) // noise canceller on
	         | _BV(ICES1) // capture on positive edge
	         | _BV(WGM12) // use CTC mode with TOP at OCR1A
	         | _BV(CS11) | _BV(CS10); // use sysclck/64 as counting freq, i.e. 250hKz or 4us ticks.
	//| _BV(CS12) | _BV(CS10); // use sysclck/1024 as counting freq
	TIMSK = _BV(TICIE1) // enable capture interrupt
	        | _BV(OCIE1A); // enable compare interruppt
	OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits


	// set up usart
#define BAUD 500000
#include <util/setbaud.h>
	UBRRH = UBRRH_VALUE;
	UBRRL = UBRRL_VALUE;
	#if USE_2X
	UCSRA |= _BV(U2X);
	#else
	UCSRA &= ~_BV(U2X);
	#endif
	UCSRB = _BV(RXEN) | _BV(TXEN);
	UCSRC = _BV(URSEL) | _BV(USBS) | (3 << UCSZ0);

	USART_Transmit('h');
	USART_Transmit('e');
	USART_Transmit('l');
	USART_Transmit('l');
	USART_Transmit('o');
	USART_Transmit(' ');
	USART_Transmit('w');
	USART_Transmit('o');
	USART_Transmit('r');
	USART_Transmit('l');
	USART_Transmit('d');
	USART_Transmit('\n');

	sei(); // make sure we get interrupts
	while (true) {
		if (gNextReadBufferIndex == 0xFF) { // no valid next read buffer
			continue;
		}
		cli(); // atomic operation to get the gNextReadBufferIndex read and set to invalid
		auto readBufferIndex = gNextReadBufferIndex;
		gNextReadBufferIndex = 0xFF;
		sei();
		ProcessingPulseTrain.fSet(true);
		for (auto index = 0; index < gBuffer[readBufferIndex].fGetNEntries(); index++) {
			unsigned short value = gBuffer[readBufferIndex].fGetEntry(index);
			USART_hexshort(value);
			USART_Transmit(' ');
		}
		USART_Transmit('\n');
		ProcessingPulseTrain.fSet(false);
		gBuffer[readBufferIndex].fClear(); // clear buffer after reading
	}
}
