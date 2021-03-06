#ifndef __usart_H_
#define __usart_H_

template <
    unsigned short csraAddr,
    unsigned short csrbAddr,
    unsigned short csrcAddr,
    unsigned short brlAddr,
    unsigned short brhAddr,
    unsigned short udrAddr,
    unsigned char bvU2X,
    unsigned char bvRXEN,
    unsigned char bvTXEN,
    unsigned char bvRXCIE,
    unsigned char bvURSEL,
    unsigned char bitUCSZ0,
    unsigned char bvUDRE,
    unsigned long BAUD,
    unsigned long BAUD_TOL = 2
    > class usartHandler {
  public:
	usartHandler() {
		constexpr unsigned long UBRR_VALUE16 = (((F_CPU) + 8UL * BAUD)
		                                        / (16UL * BAUD) - 1UL);
		unsigned short UBRR_VALUE;
		static_assert(BAUD <= 1000000, "baud rate too high");
		if (100 * (F_CPU) > (16 * (UBRR_VALUE16 + 1)) * (100 * BAUD + BAUD * BAUD_TOL)
		        || 100 * (F_CPU) < (16 * (UBRR_VALUE16 + 1)) * (100 * BAUD - BAUD * BAUD_TOL)) { // we need USE_2X
			_MMIO_BYTE(csraAddr) |= bvU2X;
			constexpr unsigned long UBRR_VALUE8 = (((F_CPU) + 4UL * BAUD)
			                                       / (8UL * BAUD) - 1UL);
			static_assert(UBRR_VALUE8 < 4096UL, "UBRR value too high");
			UBRR_VALUE = UBRR_VALUE8 & 0x0FFF;
		} else {
			static_assert(UBRR_VALUE16 < 4096UL, "UBRR value too high");
			UBRR_VALUE = UBRR_VALUE16 & 0x0FFF;
			_MMIO_BYTE(csraAddr) &= ~bvU2X;
		}
		_MMIO_BYTE(brhAddr) = UBRR_VALUE >> 8;
		_MMIO_BYTE(brlAddr) = UBRR_VALUE & 0xff;
		_MMIO_BYTE(csrbAddr) = bvRXEN | bvTXEN | bvRXCIE;
		_MMIO_BYTE(csrcAddr) = bvURSEL | (3 << bitUCSZ0);
	};

	void fTransmit( unsigned char data ) {
		/* Wait for empty transmit buffer */
		while ( !( _MMIO_BYTE(csraAddr) & bvUDRE) );
		/* Put data into buffer, sends the data */
		_MMIO_BYTE(udrAddr) = data;
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

template <
    unsigned short csraAddr,
    unsigned short csrbAddr,
    unsigned short csrcAddr,
    unsigned short brlAddr,
    unsigned short brhAddr,
    unsigned short udrAddr,
    unsigned char bvU2X,
    unsigned char bvRXEN,
    unsigned char bvTXEN,
    unsigned char bvRXCIE,
    unsigned char bvURSEL,
    unsigned char bitUCSZ0,
    unsigned char bvUDRE,
    unsigned long BAUD,
    unsigned long BAUD_TOL,
    unsigned char ringBufferSize,
    unsigned char lineBufferSize
    > class usartHandlerWithBuffer: public usartHandler<csraAddr, csrbAddr, csrcAddr, brlAddr, brhAddr,  udrAddr, bvU2X, bvRXEN, bvTXEN, bvRXCIE, bvURSEL, bitUCSZ0, bvUDRE, BAUD, BAUD_TOL> {
  public:
	enum : unsigned char { kRingBuferSize = 1 << ringBufferSize,
	                       kLineBufferSize = lineBufferSize
	                     };
	volatile unsigned char lWriteIndex;
	unsigned char lReadIndex;
	unsigned char lLineIndex;
	char lBuffer[kRingBuferSize];
	char lLine[kLineBufferSize];
	usartHandlerWithBuffer() :
		lWriteIndex(0), lReadIndex(0) {

	};
	const char* fNextLine() {
		// check if a newline is found
		unsigned char bytesInBuffer;
		while ((bytesInBuffer = (lWriteIndex - lReadIndex) & (kRingBuferSize - 1))) {
			auto value = lBuffer[lReadIndex];
			lReadIndex = (lReadIndex + 1) & (kRingBuferSize - 1);
			if (value == '\n') {
				lLine[lLineIndex] = '\0';
				lLineIndex = 0;
				return lLine;
			}
			lLine[lLineIndex++] = value;
			if (lLineIndex == kLineBufferSize) {
				lLineIndex = 0;
				return nullptr;
			}
		}
		return nullptr;
	};
	inline void fAddByteToBuffer() {
		lBuffer[lWriteIndex] = _MMIO_BYTE(udrAddr);
		lWriteIndex = (lWriteIndex + 1) & (kRingBuferSize - 1);
	};
};


#define USARTHandler(UartIndex, BaudRate, BS, LS, bvURSEL)	  \
	usartHandlerWithBuffer<																			 \
	(unsigned short)(&UCSR##UartIndex##A),											 \
	(unsigned short)(&UCSR##UartIndex##B),										 \
	(unsigned short)(&UCSR##UartIndex##C),										 \
	(unsigned short)(&UBRR##UartIndex##L),										 \
	(unsigned short)(&UBRR##UartIndex##H),										 \
	(unsigned short)(&UDR##UartIndex),												 \
	(unsigned char)(_BV(U2X##UartIndex)), \
	(unsigned char)(_BV(RXEN##UartIndex)), \
	(unsigned char)(_BV(TXEN##UartIndex)), \
	(unsigned char)(_BV(RXCIE##UartIndex)), \
	(unsigned char)(bvURSEL), \
	(unsigned char)(UCSZ##UartIndex##0),   \
	(unsigned char)(_BV(UDRE##UartIndex)), \
	BaudRate,																									 \
	2,																												 \
	BS,																												 \
	LS>


#endif
