#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <dirent.h>
#include <string.h>
#include <iostream>
class owTemperature: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface,
	        public slowcontrol::unitInterface {
  protected:
	std::string lPath;
  public:
	owTemperature(const char *aPath):
		boundCheckerInterface(0.5, -55, 125),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, "deg C") {
		std::string basePath = "/1w/";
		basePath += aPath;
		basePath += "/";
		lPath = basePath;
		lPath += "temperature";
		lDeadBand.fSetValue(0.5);
		fInitializeUid(aPath);
		fConfigure();
	};
	virtual void fReadCurrentValue() {
		std::ifstream thermometer(lPath.c_str());
		float temperature;
		thermometer >> temperature;
		if (-55 <= temperature || temperature <= 125) { // limits according to DS18B20 data sheet
			fStore(temperature);
		} else {
			std::cerr << "bad temperature " << temperature << " on " << lPath << std::endl;
		}
	};
};



int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for reading one-wire devices via owfs");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("onewired");
	{
		DIR *owdir = opendir("/1w");
		for (;;) {
			struct dirent *de = readdir(owdir);
			if (de == NULL) {
				break;
			}
			if (strncmp("10.", de->d_name, 3) == 0
			        || strncmp("28.", de->d_name, 3) == 0) {
				new owTemperature(de->d_name);
			}
		}
		closedir(owdir);
	}
	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
