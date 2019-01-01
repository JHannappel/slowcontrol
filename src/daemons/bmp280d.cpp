#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "measurement.h"
#include "filevalue.h"
#include "slowcontrolDaemon.h"
#include <Options.h>
#include <dirent.h>
#include <cmath>
#include <iostream>
#include <string.h>

class bmp280pressure:
	public slowcontrol::boundCheckerInterface<slowcontrol::filevalue::input<float, std::ratio<10, 1>>>,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::unitInterface {
  public:
	bmp280pressure(const std::string& aPath, const std::string& aName):
		boundCheckerInterface(800, 1100, aPath, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, " hPa") {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	};
	virtual bool fReadCurrentValue() {
		return input::fReadCurrentValue();
	}
};
class bmp280temperature: public slowcontrol::boundCheckerInterface<slowcontrol::filevalue::input<float, std::ratio<1, 1000>>>,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::unitInterface {
  public:
	bmp280temperature(const std::string& aPath, const std::string& aName):
		boundCheckerInterface(0, 30, aPath, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, " degC") {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	};
	virtual bool fReadCurrentValue() {
		return input::fReadCurrentValue();
	}
};



int main(int argc, const char *argv[]) {
	options::single<std::string> pressureDevice('p', "pressure-devive",
	        "name of the pressure device",
	        "/sys/bus/i2c/devices/1-0076/iio:device0/in_pressure_input");
	options::single<std::string> temperatureDevice('t', "temperature-devive",
	        "name of the temperature device",
	        "/sys/bus/i2c/devices/1-0076/iio:device0/in_temp_input");

	options::single<std::string> measurementName('n', "name", "name base of the measurement");

	options::parser parser("slowcontrol program for reading bmp280 pressure sensors");

	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("bmp280d");

	bmp280pressure pressure(pressureDevice, measurementName + "pressure");
	bmp280temperature temperature(temperatureDevice, measurementName + "temperature");

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}

