#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "iopin.h"
#define BAUD 500000

#include "usart.h"

class pulseBuffer {
  public:
	enum : unsigned char {
		kSize =  70,
		kNBuffers = 3,
		kMinEdges = 64
	};
	enum : unsigned short {
		kMinPulseLength = 0x3F,
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

IOPin(D, 7) RFDataOut(true);


ISR(TIMER1_CAPT_vect) { // an edge was detected and captured
	Capture.fSet(true);
	unsigned short interval = ICR1;
	//	bool wasPosEdge = bit_is_set(TCCR1B, ICES1);
	TCCR1B ^= _BV(ICES1); // change edge
	TIFR = _BV(ICF1); // reset interrupt flag
	TCNT1 = 0;
	ACOmonitor.fSet(bit_is_set(ACSR, ACO));
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
}

ISR(TIMER1_COMPA_vect) { // an edge timeout happened
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
}

void waitCounter0(unsigned short ticksOf4us) {
	TCCR0 = _BV(CS02); // use normal mode with 1/256 sysclk, i.e. 16us
	TCNT0 = 0xffu - (ticksOf4us >> 2);
	while ((TIFR & _BV(TOV0)) == 0) {}; // wait until overflow happened
	TIFR = _BV(TOV0); // clear overflow
}

void sendPattern(const char *aPattern) {
	RFDataOut.fSet(true);
	waitCounter0(0x99);
	RFDataOut.fSet(false);
	for (int i = 0; i < 3; i++) {
		waitCounter0(0x99);
	}
	while (*aPattern != '\0') {
		unsigned short nibble;
		if (*aPattern > 'A') {
			nibble = *aPattern - 'A' + 0x0A;
		} else {
			nibble = *aPattern - '0';
		}
		unsigned char mask = 0x08;
		while (mask != 0) {
			if ((nibble & mask) != 0) {
				RFDataOut.fSet(true);
				waitCounter0(0x135);
				RFDataOut.fSet(false);
				waitCounter0(0x99);
			} else {
				RFDataOut.fSet(true);
				waitCounter0(0x99);
				RFDataOut.fSet(false);
				waitCounter0(0x135);
			}
			mask >>= 1;
		}
		aPattern++;
	}
	RFDataOut.fSet(false);
	waitCounter0(0x3FF);
	waitCounter0(0x3FF);
	waitCounter0(0x3FF);
}

int main(void) {
	gNextReadBufferIndex = 0xFFu; // mark next read buffer as invalid


	// enable comparator and set as capture source
	ACSR = _BV(ACIC);
	TCCR1A = 0; // normal mode
	TCCR1B = _BV(ICNC1) // noise canceller on
	         | _BV(ICES1) // capture on positive edge
	         | _BV(WGM12) // use CTC mode with TOP at OCR1A
	         | _BV(CS11) | _BV(CS10); // use sysclck/64 as counting freq, i.e. 250hKz or 4us ticks.
	//| _BV(CS12) | _BV(CS10); // use sysclck/1024 as counting freq
	TIMSK = _BV(TICIE1) // enable capture interrupt
	        | _BV(OCIE1A); // enable compare interruppt
	OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits

	gUSARTHandler.fString_P(PSTR("hello world by " __FILE__ "\n"));

	bool fullOutput = false;

	sei(); // make sure we get interrupts
	while (true) {
		auto line = gUSARTHandler.fNextLine();
		if (line != nullptr) {
			gUSARTHandler.fString_P(PSTR("received '"));
			gUSARTHandler.fString(line);
			gUSARTHandler.fString_P(PSTR("'\n"));

			if (strcmp_P(line, PSTR("full")) == 0) {
				fullOutput = true;
			} else if (strcmp_P(line, PSTR("sparse")) == 0) {
				fullOutput = false;
			} else if (strncmp_P(line, PSTR("send "), 5) == 0) {
				for (int i = 0; i < 4; i++) {
					sendPattern(line + 5);
				}
			}
		}

		if (gNextReadBufferIndex == 0xFF) { // no valid next read buffer
			continue;
		}
		cli(); // atomic operation to get the gNextReadBufferIndex read and set to invalid
		auto readBufferIndex = gNextReadBufferIndex;
		gNextReadBufferIndex = 0xFF;
		sei();
		ProcessingPulseTrain.fSet(true);
		if (fullOutput) {
			for (auto index = 0; index < gBuffer[readBufferIndex].fGetNEntries(); index++) {
				unsigned short value = gBuffer[readBufferIndex].fGetEntry(index);
				gUSARTHandler.fHexShort(value);
				gUSARTHandler.fTransmit(' ');
			}
			gUSARTHandler.fString_P(PSTR("=> "));
		}
		unsigned char code[16];
		unsigned char byte = 0;
		unsigned char bit = 8;
		for (auto index = 1; index < gBuffer[readBufferIndex].fGetNEntries() - 1; index += 2) {
			code[byte] <<= 1;
			if (gBuffer[readBufferIndex].fGetEntry(index) < gBuffer[readBufferIndex].fGetEntry(index + 1)) {
				gUSARTHandler.fTransmit('0');
			} else {
				gUSARTHandler.fTransmit('1');
				code[byte] |= 0x01;
			}
			bit--;
			if (bit == 0) {
				byte++;
				bit = 8;
			}
		}
		gUSARTHandler.fTransmit(' ');
		gUSARTHandler.fHexByte(byte);
		gUSARTHandler.fTransmit(' ');
		for (int b = 0; b <= byte; b++) {
			gUSARTHandler.fHexByte(code[b]);
		}
		gUSARTHandler.fTransmit('\n');
		ProcessingPulseTrain.fSet(false);
		gBuffer[readBufferIndex].fClear(); // clear buffer after reading
	}
}
