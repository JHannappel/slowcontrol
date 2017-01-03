#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "parasitic_temperature.h"
#include <Options.h>
#include <dirent.h>
#include <string.h>
#include <iostream>

/* Puffer fuer die RTC-Daten */
#define BUFSIZE 7

/* Verwende I2C 1 */
#define I2CADDR "/dev/i2c-1"

/* defined in <linux/i2c-dev.h> */
#define I2C_SLAVE 0x703



class hih6131moisture: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface,
	        public slowcontrol::unitInterface {
  protected:
	int fd;
	slowcontrol::parasitic_temperature *lTemperature;
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
			std::string humidityName(aNameBase);
			humidityName += "_humidity";
			fInitializeUid(humidityName.c_str());
		}
		fConfigure();
	};
	virtual bool fReadCurrentValue() {
		unsigned char buf[4];
		buf[0] = 0;
		write(fd, buf, 0);
		usleep(100);
		read(fd, buf, 4);
		unsigned int hum;
		hum = buf[1];
		hum |= (buf[0] & 0x3F) << 8;
		auto valueHasChanged = fStore((100.0 * hum) / (0x4000 - 2));
		unsigned int temp;
		temp = buf[3] >> 2;
		temp |= static_cast<unsigned int>(buf[2]) << 6;
		valueHasChanged |= lTemperature->fStore((165. * temp) / (0x4000 - 2) - 40.);
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
