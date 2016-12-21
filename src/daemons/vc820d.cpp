#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "communications.h"
#include <Options.h>
#include <termios.h>
#include <sys/ioctl.h>


class dvmReadout: public slowcontrol::measurement<float>,
		  public slowcontrol::pollReaderInterface,
		  public slowcontrol::unitInterface
{
protected:
  slowcontrol::serialLine lSerial;
  unsigned char lBuffer[9];
  int lNibbleIndex;
public:
  dvmReadout(const std::string& aName, const std::string& aDevice):
    measurement(0),
    unitInterface(lConfigValues,""),
    lSerial(aDevice,2400) {
    lClassName.fSetFromString(__func__);
    fInitializeUid(aName);
    fConfigure();
    lNibbleIndex=0;
    {
      int arg;
      arg = TIOCM_RTS | TIOCM_DTR;
      ioctl(fGetFd(), TIOCMBIS, &arg);
      
      arg = TIOCM_RTS;
      ioctl(fGetFd(), TIOCMBIC, &arg);
    }
  }
  int fDecodeDigit(unsigned char aPattern) {
    switch (aPattern & 0x7f) {
    case 0xd7: return 0;
    case 0x05: return 1;
    case 0x5b: return 2;
    case 0x1f: return 3;
    case 0x27: return 4;
    case 0x3e: return 5;
    case 0x7e: return 6;
    case 0x15: return 7;
    case 0x7f: return 8;
    case 0x3f: return 9;
    default:
      return -1;
    }
  }

  virtual int fGetFd() {
    return lSerial.fGetFd();
  }
  virtual void fSetPollFd(struct pollfd *aPollfd) {
    aPollfd->fd = fGetFd();
    aPollfd->events = POLLIN;
  }
  virtual void fProcessData(short /*aRevents*/) {
    unsigned char c;
    if (read(fGetFd(), &c, 1) == 1) {
      auto position = (c >> 4) & 0x0fu;
      if (position == 1) { // start of a telegram
	memset(lBuffer,0,sizeof(lBuffer));
	lNibbleIndex = 1;
      } else {
	lNibbleIndex++;
	if (lNibbleIndex != position) { // bad datagram
	  return;
	}
      }
      if (position & 1) { // odd part
	lBuffer[position/2] |= c & 0x0f;
      } else {
	lBuffer[position/2] |= (c & 0x0f) << 4;
      }
      if (position == 0x0e) { // end of a telegram
	float value = 1000.0 * fDecodeDigit(lBuffer[1]) +
	  100.0 * fDecodeDigit(lBuffer[2]) +
	  10.0 * fDecodeDigit(lBuffer[3]) +
	  1.0 * fDecodeDigit(lBuffer[4]);
	if (lBuffer[4] & 0x80) { // decimal dot
	  value /= 10.0;
	} else if (lBuffer[3] & 0x80) {
	  value /= 100.0;
	} else if (lBuffer[2] & 0x80) {
	  value /= 100.0;
	}
	if (lBuffer[1] & 0x80) {
	  value = -value;
	}
	if (lBuffer[5] & 0x08) { // prefix: milli
	  value *= 0.001;
	} else if (lBuffer[5] & 0x80) { // prefix: micro
	  value *= 0.000001;
	} else if (lBuffer[5] & 0x40) { // prefix: nano
	  value *= 0.000000001;
	} else if (lBuffer[5] & 0x02) { // prefix: Mega
	  value *= 1000000;
	} else if (lBuffer[5] & 0x20) { // prefix: kilo
	  value *= 1000;
	}
	fStore(value);
	const char *unit="";
	if (lBuffer[6] & 0x08) {
	  unit = "A";
	}  else if (lBuffer[6] & 0x02) {
	  unit = "Hz";
	} else if (lBuffer[5] & 0x04) {
	  unit = "%";
	} else if (lBuffer[7] & 0x10) {
	  unit = "deg C";
	} else if (lBuffer[6] & 0x40) {
	  unit = "Ohm";
	} else if (lBuffer[6] & 0x04) {
	  unit = "V";
	} else if (lBuffer[6] & 0x80) {
	  unit = "F";
	}
	if (lUnit.fGetValue().compare(unit)!=0) {
	  lUnit.fSetValue(unit);
	  fSaveOption(lUnit,"change from readout");
	}
      }
    }
  }
};


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for readingc VC820/840 DVMs");
	OptionMap<std::string> dvms('d', "device", "DVM name/device pairs");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("vc820d");

	for (auto it : dvms) {
		new dvmReadout(it.first, it.second);
	}

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
