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
	enum : unsigned char { kBufSize = 1 << bufferSize};
	volatile unsigned char lWriteIndex;
	unsigned char lReadIndex;
	unsigned char lLineIndex;
	char lBuffer[kBufSize];
	char lLine[kBufSize];
	usartHandlerWithBuffer() :
		lWriteIndex(0), lReadIndex(0) {
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
		unsigned char bytesInBuffer;
		while ((bytesInBuffer = (lWriteIndex - lReadIndex) & (kBufSize - 1))) {
			auto value = lBuffer[lReadIndex];
			lReadIndex = (lReadIndex + 1) & (kBufSize - 1);
			if (value == '\n') {
				lLine[lLineIndex] = '\0';
				fTransmit('w');
				fHexByte(lWriteIndex);
				fTransmit('r');
				fHexByte(lReadIndex);
				fTransmit('b');
				fHexByte(bytesInBuffer);
				fTransmit('l');
				fHexByte(lLineIndex);
				fTransmit('\n');
				lLineIndex = 0;
				return lLine;
			}
			lLine[lLineIndex++] = value;
			if (lLineIndex == kBufSize) {
				lLineIndex = 0;
				return nullptr;
			}
		}
		return nullptr;
	};
	inline void fAddByteToBuffer(unsigned char byte) {
		lBuffer[lWriteIndex] = byte;
		lWriteIndex = (lWriteIndex + 1) & (kBufSize - 1);
	};
};

usartHandlerWithBuffer<7> gUSARTHandler;

ISR(USARTRXC_vect) {
	gUSARTHandler.fAddByteToBuffer(UDR);
}
#endif
