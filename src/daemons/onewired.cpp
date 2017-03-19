#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <dirent.h>
#include <iostream>
#include <string.h>


static void populateThermometers();

class owTemperature: public slowcontrol::boundCheckerDamper<slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>>,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::unitInterface {
  protected:
	std::string lPath;
	measurement_state::stateType lBadFileType = 0;
  public:
	owTemperature(const char *aPath):
		boundCheckerDamper(std::chrono::seconds(120), -55, 125, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, "deg C") {
		lClassName.fSetFromString(__func__);
		std::string basePath = "/1w/";
		basePath += aPath;
		basePath += "/";
		lPath = basePath;
		lPath += "temperature";
		lDeadBand.fSetValue(0.5);
		fInitializeUid(aPath);
		fConfigure();
	};
	bool fReadCurrentValue() override {
		bool valueHasChanged = false;
		std::ifstream thermometer(lPath.c_str());
		if (thermometer.fail() && lState != lBadFileType) {
			lBadFileType = fSetState("unreadable", "can't open file" + lPath);
		} else {
			float temperature;
			thermometer >> temperature;
			if (-55 <= temperature || temperature <= 125) { // limits according to DS18B20 data sheet
				valueHasChanged = fStore(temperature);
			} else {
				std::cerr << "bad temperature " << temperature << " on " << lPath << std::endl;
			}
			if (lBadFileType != 0 && lState != lBadFileType) {
				// probably recovered from bus problems which might hint at
				// new devices connected.
				populateThermometers();
			}
		}
		return valueHasChanged;
	};
};

static void populateThermometers() {
	static std::set<std::string> knownThermometers;

	DIR *owdir = opendir("/1w");
	for (;;) {
		struct dirent *de = readdir(owdir);
		if (de == nullptr) {
			break;
		}
		if (strncmp("10.", de->d_name, 3) == 0
		        || strncmp("28.", de->d_name, 3) == 0) {
			std::string thermometer(de->d_name);
			auto found = knownThermometers.insert(thermometer);
			if (found.second == true) { // if we inserted we do not now this one yet
				new owTemperature(de->d_name);
			}
		}
	}
	closedir(owdir);
}


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for reading one-wire devices via owfs");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("onewired");

	populateThermometers();

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
