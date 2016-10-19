

template <unsigned short portAddr,
          unsigned short ddrAddr,
          unsigned short inputPinsAddr,
          unsigned char bit> class ioPin {
  public:
	ioPin() {};
	ioPin(bool isOutput) {
		if (isOutput) {
			_MMIO_BYTE(ddrAddr) |= _BV(bit);
		} else {
			_MMIO_BYTE(ddrAddr) &= ~_BV(bit);
		}
	};
	void fSet(bool value) {
		if (value) {
			_MMIO_BYTE(portAddr) |= _BV(bit);
		} else {
			_MMIO_BYTE(portAddr) &= ~_BV(bit);
		}
	};
	bool fGet() {
		return (_MMIO_BYTE(inputPinsAddr) & _BV(bit) != 0);
	}
};

#define IOPin(Port,Bit) ioPin<(unsigned short)(&PORT##Port),(unsigned short)(&DDR##Port),(unsigned short)(&PIN##Port),Bit>
