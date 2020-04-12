#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "measurement.h"
#include "gpio.h"
#include "slowcontrolDaemon.h"

#include <Options.h>
#include <iostream>
#include <string.h>
/* Puffer fuer die RTC-Daten */
#define BUFSIZE 7

/* Verwende I2C 1 */
#define I2CADDR "/dev/i2c-1"

/* defined in <linux/i2c-dev.h> */
#define I2C_SLAVE 0x703
class bh1750brightness: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface,
	        public slowcontrol::unitInterface {
  protected:
	int fd;
  public:
	bh1750brightness(const char *aPath, const char *aName):
		boundCheckerInterface(40, 60, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(3)),
		unitInterface(lConfigValues, " lx") {
		lClassName.fSetFromString(__func__);
		fd = open(aPath, O_RDWR);
		ioctl(fd, I2C_SLAVE, 0x23);
		fInitializeUid(aName);
		fConfigure();
	};
	bool fReadCurrentValue() override {
		unsigned char buf[4];
		buf[0] = 0x01;
		if (write(fd, buf, 1) < 0) {
			throw slowcontrol::exception("can't wake bh1750", slowcontrol::exception::level::kStop);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		buf[0] = 0x4F;
		if (write(fd, buf, 1) < 0) {
			throw slowcontrol::exception("can't set bh1750 mt high nibble", slowcontrol::exception::level::kStop);
		}
		buf[0] = 0x6E;
		if (write(fd, buf, 1) < 0) {
			throw slowcontrol::exception("can't set bh1750 mt low nibble", slowcontrol::exception::level::kStop);
		}
		buf[0] = 0x21;
		if (write(fd, buf, 1) < 0) {
			throw slowcontrol::exception("can't start bh1750", slowcontrol::exception::level::kStop);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(800));
		if (read(fd, buf, 2) < 2) {
			throw slowcontrol::exception("can't read 2 bytes from bh1750", slowcontrol::exception::level::kContinue);
		}
		unsigned short brightness;
		brightness = buf[1];
		brightness |= buf[0] << 8;
		return fStore(brightness);
	};
};

class displayBrighnessRead: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<short>, false, true>,
	public slowcontrol::defaultReaderInterface {
  protected:
	std::string lPath;
  public:
	displayBrighnessRead(const char *aPath):
		boundCheckerInterface(10, 255, 2),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		lPath(aPath) {
		lClassName.fSetFromString(__func__);
		std::string name;
		name = slowcontrol::base::fGetHostName();
		name += ":backlight_read";
		lDeadBand.fSetValue(1);
		fInitializeUid(name);
		fConfigure();
	};
	bool fReadCurrentValue() override {
		std::ifstream file(lPath.c_str());
		short value;
		file >> value;
		return fStore(value);
	};
};
class displayBrighnessWrite: public slowcontrol::measurement<short>,
	public slowcontrol::writeValueWithType<short> {
  protected:
	std::string lPath;
  public:
	displayBrighnessWrite(const char *aPath):
		lPath(aPath) {
		lClassName.fSetFromString(__func__);
		std::string name;
		name = slowcontrol::base::fGetHostName();
		name += ":backlight_write";
		fInitializeUid(name);
		fConfigure();
	};
	void fSet(short value) {
		std::ofstream file(lPath.c_str());
		file << value;
		fStore(value);
	};
	bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) override {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			fSet(req->lGoalValue);
			aResponse = "done.";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	};
};





int main(int argc, const char *argv[]) {
	options::single<const char*> deviceName('d', "device", "name of the i2c device", "/dev/i2c-1");
	options::single<const char*> measurementName('n', "name", "name base of the measurement");
	options::single<unsigned int> motionPin('m', "motionPin", "gpiopin for motion detector", 27);
	options::single<std::string> startCmd('s', "startCmd", "startCommand");
	options::single<std::string> stopCmd('S', "stopCmd", "startCommand");


	options::parser parser("slowcontrol program for reading bh1750 brightness sensors");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("rpiBrightnessd");

	bh1750brightness bh1750(deviceName, measurementName);

	displayBrighnessWrite bw("/sys/class/backlight/rpi_backlight/brightness");

	displayBrighnessRead br("/sys/class/backlight/rpi_backlight/actual_brightness");

	slowcontrol::watch_pack watchPack;
	slowcontrol::watched_measurement<slowcontrol::gpio::input_value>  motionDet(watchPack,

	[](slowcontrol::gpio::input_value * aThat) {
		return aThat->fGetCurrentValue();
	},
	"DisplayPIRSense", motionPin.fGetValue());


	daemon->fStartThreads();

	auto lastPresenceTime = std::chrono::system_clock::now();
	bool present = false;
	while (!daemon->fGetStopRequested()) {
		if (watchPack.fWaitForChange()) { // motion detected
			lastPresenceTime = std::chrono::system_clock::now();
			if (!present) { // someone just appeared
				present = true;
				system(startCmd.c_str());
			}
		} else if (present) { // no motion, just timeout (or presence vanished)
			auto now = std::chrono::system_clock::now();
			if (now - lastPresenceTime > std::chrono::seconds(60)) {
				present = false;
				system(stopCmd.c_str());
				bw.fSet(0);
			}
		}
		if (present) {
			short requestBrighness = 12; // minimum non-zero value
			requestBrighness += bh1750.fGetCurrentValue();
			if (requestBrighness > 255) {
				requestBrighness = 255;
			}
			bw.fSet(requestBrighness);
		}
	}

	daemon->fWaitForThreads();
}
