#ifndef __iopin_H_
#define __iopin_H_
#include <type_traits>

/// template class for ioPins

/// provides objects without data members for dealing with IOpins
/// initialisation like setting port direction is done in the
/// constructor, so when the IOpins are defined as global objects
/// they are initialized at startup.
/// The use of the template mechanism removes at run time overhead,
/// so using this class is as efficient as direct accesses, but without
/// the risk to mess up the bits.
/// instances of this class should be created with the define IOpin macro
template <unsigned char * portAddr,
          unsigned char * ddrAddr,
          unsigned char * inputPinsAddr,
          unsigned char bit> class ioPin {
  public:
	ioPin(bool isOutput) {
		if (isOutput) {
			*ddrAddr |= _BV(bit);
		} else {
			*ddrAddr &= ~_BV(bit);
		}
	};
	/// set pin to high(true) or low(false)
	void fSet(bool value) {
		if (value) {
			*portAddr |= _BV(bit);
		} else {
			*portAddr &= ~_BV(bit);
		}
	};
	bool fGet() { /// read pin value, high as true, low as false
		return (*inputPinsAddr & _BV(bit) != 0);
	}
};

/// macro to define an ioPin, uses preprocessor magic to produce the
/// required register adresses from the symbolic port name.
/// \class ioPin
#define portbla(x) std::remove_pointer<decltype(x)>(x)
#define IOPin(Port,Bit) ioPin<portbla(PORT##Port),portbla(DDR##Port),portbla(PIN##Port),Bit>

#endif
