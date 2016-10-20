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
	unsigned char lBytesInBuffer;
	unsigned char lWriteIndex;
	unsigned char lReadIndex;
	volatile unsigned char lNLines;
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
		if (lNLines == 0) {
			return nullptr;
		}
		cli();
		unsigned char i;
		for (i = 0; lBytesInBuffer > 0 && i < sizeof(lLine) - 1; i++) {
			lLine[i] = lBuffer[lReadIndex];
			lBuffer[lReadIndex] = '\0';
			lBytesInBuffer--;
			lReadIndex = (lReadIndex + 1) % kBufSize;
			if (lLine[i] == '\0') {
				break;
			}
		}
		lLine[i] = '\0';
		lNLines--;
		sei();
		return lLine;
	};
};

usartHandlerWithBuffer<128> gUSARTHandler;

ISR(USARTRXC_vect) {
	cli();
	auto byte = UDR;
	if (byte == '\n') {
		byte = '\0';
		gUSARTHandler.lNLines++;
	}
	gUSARTHandler.lBuffer[gUSARTHandler.lWriteIndex] = byte;
	gUSARTHandler.lWriteIndex = (gUSARTHandler.lWriteIndex + 1) % gUSARTHandler.fGetBufSize();
	gUSARTHandler.lBytesInBuffer++;
	sei();
}
#endif
