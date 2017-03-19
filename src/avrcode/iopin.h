#ifndef __iopin_H_
#define __iopin_H_


/// template class for ioPins

/// provides objects without data members for dealing with IOpins
/// initialisation like setting port direction is done in the
/// constructor, so when the IOpins are defined as global objects
/// they are initialized at startup.
/// The use of the template mechanism removes at run time overhead,
/// so using this class is as efficient as direct accesses, but without
/// the risk to mess up the bits.
/// instances of this class should be created with the define IOpin macro
template <unsigned short portAddr,
          unsigned short ddrAddr,
          unsigned short inputPinsAddr,
          unsigned char bit> class ioPin {
  public:
	ioPin(bool isOutput) {
		if (isOutput) {
			_MMIO_BYTE(ddrAddr) |= _BV(bit);
		} else {
			_MMIO_BYTE(ddrAddr) &= ~_BV(bit);
		}
	};
	/// set pin to high(true) or low(false)
	void fSet(bool value) {
		if (value) {
			_MMIO_BYTE(portAddr) |= _BV(bit);
		} else {
			_MMIO_BYTE(portAddr) &= ~_BV(bit);
		}
	};
	bool fGet() { /// read pin value, high as true, low as false
		return (_MMIO_BYTE(inputPinsAddr) & _BV(bit) != 0);
	}
};

/// macro to define an ioPin, uses preprocessor magic to produce the
/// required register adresses from the symbolic port name.
/// \class ioPin
#define IOPin(Port,Bit) ioPin<reinterpret_cast<unsigned short>(&PORT##Port),reinterpret_cast<unsigned short>(&DDR##Port),reinterpret_cast<unsigned short>(&PIN##Port),Bit>

#endif
