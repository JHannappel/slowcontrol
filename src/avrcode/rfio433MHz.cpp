#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "iopin.h"
#define BAUD 125000

#include "usart.h"

class pulseBuffer {
  public:
	enum : unsigned char {
		kSize =  70,
		kNBuffers = 4,
		kMinEdges = 44
	};
	enum : unsigned short {
		kMinPulseLength = 0x3F,
		kMaxCountValue = 0x7FFF,
		kValueMask = 0x7FFF
	};
  private:
	unsigned short lPulses[kSize];
	unsigned char lIndex;
	volatile bool lValid;
  public:
	pulseBuffer(): lIndex(0) {
	};
	bool fAddPulse(unsigned short aTime, bool isNowHigh) {
		if (aTime < kMinPulseLength) {
			return false;
		}
		if (isNowHigh) {
			aTime |= ~kValueMask;
		}
		lPulses[lIndex++] = aTime;
		if (lIndex >= kSize) {
			return (false);
		}
		return (true);
	};
	bool fIsValid() const {
		return lValid;
	};
	void fSetValid() {
		lValid = true;
	};
	void fClear() {
		lIndex = 0;
		lValid = false;
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
bool gFullOutput = false;

IOPin(C, 0) ACOmonitor(true);
IOPin(C, 1) AquiringPulseTrain(true);
IOPin(C, 2) ProcessingPulseTrain(true);
IOPin(C, 3) TimeOut(true);
IOPin(C, 4) Capture(true);

IOPin(D, 7) RFDataOut(true);


ISR(TIMER1_CAPT_vect) { // an edge was detected and captured
	Capture.fSet(true);
	unsigned short interval = ICR1;
	TCCR1B ^= _BV(ICES1); // change edge
	TIFR = _BV(ICF1); // reset interrupt flag
	TCNT1 = 0;
	bool isHigh = bit_is_set(ACSR, ACO);
	ACOmonitor.fSet(isHigh);
	if (gBuffer[gWriteBufferIndex].fAddPulse(interval, isHigh) == false) { // probably an overflow
		gBuffer[gWriteBufferIndex].fClear();
		OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits
		TCCR1B |= _BV(ICES1); // wait for a positive edge
		AquiringPulseTrain.fSet(false);
	} else if (gBuffer[gWriteBufferIndex].fGetNEntries() == 2) {
		// set timeout for end of pulse train recognition to average of
		// the start and pause pulse at the biginning of the train
		OCR1A = ((gBuffer[gWriteBufferIndex].fGetEntry(0) & pulseBuffer::kValueMask) +
		         (gBuffer[gWriteBufferIndex].fGetEntry(1) & pulseBuffer::kValueMask)) >> 1;
	} else {
		AquiringPulseTrain.fSet(true);
	}
	Capture.fSet(false);
}

ISR(TIMER1_COMPA_vect) { // an edge timeout happened
	TimeOut.fSet(true);
	if (gBuffer[gWriteBufferIndex].fGetNEntries() > pulseBuffer::kMinEdges /* pulse train found */) {
		gBuffer[gWriteBufferIndex].fSetValid();
		gWriteBufferIndex = (gWriteBufferIndex + 1) % pulseBuffer::kNBuffers;
	} else { /* maybe junk in the buffer, clear it */
		gBuffer[gWriteBufferIndex].fClear();
	}
	OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits
	TCCR1B |= _BV(ICES1); // wait for a positive edge
	AquiringPulseTrain.fSet(false);
	TimeOut.fSet(false);
}

void waitCounter0(unsigned short aMicroSeconds) {
	TCCR0 = _BV(CS02); // use normal mode with 1/256 sysclk, i.e. 16us
	auto clockTicks = aMicroSeconds >> 4;
	unsigned char highPart = clockTicks >> 8;
	for (; highPart > 0; highPart--) {
		TCNT0 = 0;
		while ((TIFR & _BV(TOV0)) == 0) {}; // wait until overflow happened
		TIFR = _BV(TOV0); // clear overflow
	}
	unsigned char lowPart = clockTicks & 0x00FFu;
	TCNT0 = 0xffu - lowPart;
	while ((TIFR & _BV(TOV0)) == 0) {}; // wait until overflow happened
	TIFR = _BV(TOV0); // clear overflow
}

unsigned char HexToNibble(char code) {
	if (code >= 'A' && code <= 'F') {
		return code - 'A' + 0x0A;
	} else if (code >= '0' && code <= '9') {
		return code - '0';
	}
	return 0xFF;
}

unsigned char HexToByte(const char **aCode) {
	unsigned char value = 0;
	while (true) {
		auto byte = *((*aCode)++);
		auto nibble = HexToNibble(byte);
		if (nibble == 0xFF) {
			return value;
		}
		value = (value << 4) | nibble;
	}
}

unsigned short HexToShort(const char **aCode) {
	unsigned short value = 0;
	while (true) {
		auto byte = *((*aCode)++);
		auto nibble = HexToNibble(byte);
		if (nibble == 0xFF) {
			return value;
		}
		value = (value << 4) | nibble;
	}
}

void sendPattern(const char *aPattern) {
	auto nPulses = HexToByte(&aPattern);
	auto period = HexToShort(&aPattern);
	gUSARTHandler.fHexShort(period);
	gUSARTHandler.fTransmit(' ');
	gUSARTHandler.fString(aPattern);
	gUSARTHandler.fTransmit(' ');
	RFDataOut.fSet(true);
	waitCounter0(period);
	RFDataOut.fSet(false);
	waitCounter0(10 * period);
	unsigned char pulsesSent = 0;
	while (*aPattern != '\0') {
		unsigned short nibble;
		nibble = HexToNibble(*aPattern);
		if (nibble == 0xFF) {
			break;
		}
		unsigned char mask = 0x08;
		while (mask != 0) {
			if ((nibble & mask) != 0) {
				RFDataOut.fSet(true);
				waitCounter0(period * 2);
				RFDataOut.fSet(false);
				waitCounter0(period);
			} else {
				RFDataOut.fSet(true);
				waitCounter0(period);
				RFDataOut.fSet(false);
				waitCounter0(period * 2);
			}
			mask >>= 1;
			pulsesSent++;
			if (pulsesSent == nPulses) {
				break;
			}
		}
		aPattern++;
	}
	RFDataOut.fSet(false);
	gUSARTHandler.fTransmit('-');
	gUSARTHandler.fHexByte(pulsesSent);
	gUSARTHandler.fTransmit('\n');
	waitCounter0(0x3FF);
	waitCounter0(0x3FF);
	waitCounter0(0x3FF);
}

void decodePulses(pulseBuffer *aBuffer) {
	ProcessingPulseTrain.fSet(true);
	if (gFullOutput) {
		for (auto index = 0; index < aBuffer->fGetNEntries(); index++) {
			unsigned short value = aBuffer->fGetEntry(index);
			gUSARTHandler.fHexShort(value);
			gUSARTHandler.fTransmit(' ');
		}
		gUSARTHandler.fString_P(PSTR("=> "));
	}
	unsigned char code[16];
	unsigned char byte = 0;
	unsigned char bit = 8;
	unsigned short pulse = 0;
	unsigned short nPulses = 0;
	for (auto index = 1; index < aBuffer->fGetNEntries() - 1; index += 2) {
		code[byte] <<= 1;
		auto firstpulse = aBuffer->fGetEntry(index);
		auto secondpulse = aBuffer->fGetEntry(index + 1);
		firstpulse &= pulseBuffer::kValueMask;
		secondpulse &= pulseBuffer::kValueMask;

		if (firstpulse < secondpulse) {
			pulse += firstpulse;
			nPulses++;
			gUSARTHandler.fTransmit('0');
		} else {
			pulse += secondpulse;;
			nPulses++;
			gUSARTHandler.fTransmit('1');
			code[byte] |= 0x01;
		}
		bit--;
		if (bit == 0) {
			byte++;
			bit = 8;
		}
	}
	pulse /= nPulses;
	gUSARTHandler.fTransmit(' ');
	gUSARTHandler.fHexByte(nPulses);
	gUSARTHandler.fTransmit(' ');
	gUSARTHandler.fHexShort(pulse << 2);
	gUSARTHandler.fTransmit(' ');
	for (int b = 0; b < byte; b++) {
		gUSARTHandler.fHexByte(code[b]);
	}
	gUSARTHandler.fTransmit('\n');
	ProcessingPulseTrain.fSet(false);
	if (aBuffer->fIsValid()) {
		aBuffer->fClear(); // clear buffer after reading
	} else {
		gUSARTHandler.fString_P(PSTR("pulse buffer overrun"));
	}
}


int main(void) {
	// enable comparator and set as capture source
	ACSR = _BV(ACIC);
	TCCR1A = 0; // normal mode
	TCCR1B = _BV(ICNC1) // noise canceller on
	         | _BV(ICES1) // capture on positive edge
	         | _BV(WGM12) // use CTC mode with TOP at OCR1A
	         | _BV(CS11) | _BV(CS10); // use sysclck/64 as counting freq, i.e. 250hKz or 4us ticks.
	TIMSK = _BV(TICIE1) // enable capture interrupt
	        | _BV(OCIE1A); // enable compare interruppt
	OCR1A = pulseBuffer::kMaxCountValue; // set max counter value to 15 bits

	gUSARTHandler.fString_P(PSTR("hello world by " __FILE__ "\n"));

	unsigned char readBufferIndex = 0;
	sei(); // make sure we get interrupts
	while (true) {
		auto line = gUSARTHandler.fNextLine();
		if (line != nullptr) {
			gUSARTHandler.fString_P(PSTR("received '"));
			gUSARTHandler.fString(line);
			gUSARTHandler.fString_P(PSTR("'\n"));

			if (strcmp_P(line, PSTR("full")) == 0) {
				gFullOutput = true;
			} else if (strcmp_P(line, PSTR("sparse")) == 0) {
				gFullOutput = false;
			} else if (strncmp_P(line, PSTR("send "), 5) == 0) {
				for (int i = 0; i < 4; i++) {
					sendPattern(line + 5);
				}
			}
		}
		if (gBuffer[readBufferIndex].fIsValid()) {
			gUSARTHandler.fHexByte(readBufferIndex);
			gUSARTHandler.fTransmit(' ');
			gUSARTHandler.fHexByte(gWriteBufferIndex);
			gUSARTHandler.fTransmit(' ');
			gUSARTHandler.fHexByte(gBuffer[readBufferIndex].fGetNEntries());
			gUSARTHandler.fTransmit(' ');
			decodePulses(&(gBuffer[readBufferIndex]));

			readBufferIndex = (readBufferIndex + 1) % pulseBuffer::kNBuffers;
		}
	}
}
