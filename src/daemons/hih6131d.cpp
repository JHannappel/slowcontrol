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
#include "parasitic_temperature.h"
#include "slowcontrolDaemon.h"
#include <Options.h>
#include <dirent.h>
#include <cmath>
#include <iostream>
#include <string.h>
/* Puffer fuer die RTC-Daten */
#define BUFSIZE 7

/* Verwende I2C 1 */
#define I2CADDR "/dev/i2c-1"

/* defined in <linux/i2c-dev.h> */
#define I2C_SLAVE 0x703

class dewPoint: slowcontrol::measurement<float>,
	public slowcontrol::unitInterface {
  public:
	dewPoint(const char *lName):
		unitInterface(lConfigValues, "deg C") {
		lClassName.fSetFromString(__func__);
		fInitializeUid(lName);
		fConfigure();
	};
	void fCalculateAndStore(float T, float RH) {
		// dew point according to https://en.wikipedia.org/wiki/Dew_point
		// constexpr float a = 6.1121; // would be needed for sat. vapour pressure
		constexpr float b = 18.678;
		constexpr float c = 257.14;
		constexpr float d = 234.5;
		auto gamma = std::log(RH * 0.01 * std::exp((b - T / d) * (T / (c + T))));
		fStore(c * gamma / (b - gamma));
	};
};
class hih6131moisture: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface,
	        public slowcontrol::unitInterface {
  protected:
	int fd;
	slowcontrol::parasitic_temperature *lTemperature;
	dewPoint *lDewPoint;
  public:
	hih6131moisture(const char *aPath, const char *aNameBase):
		boundCheckerInterface(40, 60, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, " %") {
		lClassName.fSetFromString(__func__);
		fd = open(aPath, O_RDWR);
		ioctl(fd, I2C_SLAVE, 0x27);
		{
			std::string temperatureName(aNameBase);
			temperatureName += "_temperature";
			lTemperature = new slowcontrol::parasitic_temperature(temperatureName.c_str());
		}
		{
			std::string dewPointName(aNameBase);
			dewPointName += "_dewPoint";
			lDewPoint = new dewPoint(dewPointName.c_str());
		}
		{
			std::string humidityName(aNameBase);
			humidityName += "_humidity";
			fInitializeUid(humidityName.c_str());
		}
		fConfigure();
	};
	bool fReadCurrentValue() override {
		unsigned char buf[4];
		buf[0] = 0;
		if (write(fd, buf, 0) < 0) {
			throw slowcontrol::exception("can't trigger hih6131 readout", slowcontrol::exception::level::kStop);
		}
		usleep(100);
		if (read(fd, buf, 4) < 4) {
			throw slowcontrol::exception("can't read 4 bytes from hih6131", slowcontrol::exception::level::kContinue);
		}
		unsigned int hum;
		hum = buf[1];
		hum |= (buf[0] & 0x3F) << 8;
		auto RH = (100.0 * hum) / (0x4000 - 2);
		auto valueHasChanged = fStore(RH);
		unsigned int temp;
		temp = buf[3] >> 2;
		temp |= static_cast<unsigned int>(buf[2]) << 6;
		auto T = (165. * temp) / (0x4000 - 2) - 40.;
		valueHasChanged |= lTemperature->fStore(T);
		lDewPoint->fCalculateAndStore(T, RH);
		return valueHasChanged;
	};
};

int main(int argc, const char *argv[]) {
	Option<const char*> deviceName('d', "device", "name of the i2c device", "/dev/i2c-1");
	Option<const char*> measurementName('n', "name", "name base of the measurement");
	OptionParser parser("slowcontrol program for reading hih6131 moisture sensors");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("hih6131d");

	new hih6131moisture(deviceName, measurementName);

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
