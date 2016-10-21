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
	char lLine[bufferSize];
	volatile unsigned char lBytesInBuffer;
	volatile unsigned char lWriteIndex;
	unsigned char lReadIndex;
	usartHandlerWithBuffer() : lBytesInBuffer(0),
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
		unsigned char bytesInBuffer = lBytesInBuffer;
		unsigned char readIndex = lReadIndex;
		for (unsigned char i = 0; i < bytesInBuffer; i++) {
			auto value = lBuffer[readIndex];
			readIndex = (readIndex + 1) % kBufSize;
			if (value == '\n') {
				lLine[i++] = '\0';
				cli();
				// we copied out i bytes, subtract them from the current value
				lBytesInBuffer -= i;
				lReadIndex = readIndex;
				sei();
				return lLine;
			}
			lLine[i] = value;
		}
		return nullptr;
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
