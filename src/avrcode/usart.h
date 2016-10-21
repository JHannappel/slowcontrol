#ifndef __usart_H_
#define __usart_H_

class usartHandler {
  public:
	void fTransmit( unsigned char data ) {
		/* Wait for empty transmit buffer */
		while ( !( UCSRA & (1 << UDRE)) );
		/* Put data into buffer, sends the data */
		UDR = data;
	};

	void fHexNibble(unsigned char nibble) {
		nibble &= 0x0f;
		if (nibble < 0x0A) {
			fTransmit('0' + nibble);
		} else {
			fTransmit('A' + nibble - 0x0A);
		}
	};
	void fHexByte(unsigned char byte) {
		fHexNibble((byte >> 4) & 0x0f);
		fHexNibble(byte & 0x0f);
	};
	void fHexShort(unsigned short value) {
		fHexByte((value >> 8) & 0xff);
		fHexByte(value & 0xff);
	};
	void fString(const char *addr) {
		char c;
		while ((c = *addr++)) {
			fTransmit(c);
		}
	};
	void fString_P(const char *addr) {
		char c;
		while ((c = pgm_read_byte(addr++))) {
			fTransmit(c);
		}
	};
};

template <unsigned char bufferSize> class usartHandlerWithBuffer: public usartHandler {
  public:
	enum : unsigned char { kBufSize = bufferSize};
	char lBuffer[bufferSize];
	char lLine[64];
	volatile unsigned char lBytesInBuffer;
	volatile unsigned char lWriteIndex;
	unsigned char lReadIndex;
	usartHandlerWithBuffer() {
#include <util/setbaud.h>
		UBRRH = UBRRH_VALUE;
		UBRRL = UBRRL_VALUE;
		#if USE_2X
		UCSRA |= _BV(U2X);
		#else
		UCSRA &= ~_BV(U2X);
		#endif
		UCSRB = _BV(RXEN) | _BV(TXEN) | _BV(RXCIE);
		UCSRC = _BV(URSEL) | (3 << UCSZ0);

	};
	unsigned char fGetBufSize() const {
		return kBufSize;
	};
	const char* fNextLine() {
		// check if a newline is found
		unsigned char bytesInBuffer = lBytesInBuffer;
		bool foundNewline = false;
		unsigned char readIndex = lReadIndex;
		unsigned char i;
		for (i = 0; i < bytesInBuffer; i++) {
			if (lBuffer[readIndex] == '\n') {
				foundNewline = true;
				break;
			}
			lLine[i] = lBuffer[readIndex];
			readIndex = (readIndex + 1) % kBufSize;
		}
		if (!foundNewline) {
			return nullptr;
		}
		lLine[i] = '\0';
		cli();
		lBytesInBuffer -= i;
		lReadIndex = readIndex;
		sei();
		return lLine;
	};
};

usartHandlerWithBuffer<128> gUSARTHandler;

ISR(USARTRXC_vect) {
	auto byte = UDR;
	gUSARTHandler.lBuffer[gUSARTHandler.lWriteIndex] = byte;
	gUSARTHandler.lWriteIndex = (gUSARTHandler.lWriteIndex + 1) % gUSARTHandler.fGetBufSize();
	gUSARTHandler.lBytesInBuffer++;
}
#endif
