#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <dirent.h>
#include <iostream>
#include <string.h>
#include <list>


static void populateThermometers();

class owTemperature: public slowcontrol::boundCheckerDamper<slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>>,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::unitInterface {
  protected:
	std::string lPath;
	measurement_state::stateType lBadFileType = 0;
	std::vector<float> medianBuf;
  public:
	owTemperature(const char *aPath):
		boundCheckerDamper(std::chrono::seconds(120), -55, 125, 0.5),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		unitInterface(lConfigValues, "deg C") {
		lClassName.fSetFromString(__func__);
		std::string basePath = "/1w/uncached/";
		basePath += aPath;
		basePath += "/";
		lPath = basePath;
		lPath += "temperature";
		lDeadBand.fSetValue(0.5);
		fInitializeUid(aPath);
		fConfigure();
	};
	bool fReadCurrentValue()  {
		return fReadCurrentValueMedian(0);
	}
	bool fReadCurrentValueMedian(int step)  {
		bool valueHasChanged = false;
		std::ifstream thermometer(lPath.c_str());
		if (thermometer.fail()) {
			std::cerr << "fail on " << lPath << "\n";
			if (lState != lBadFileType) {
				lBadFileType = fSetState("unreadable", "can't open file" + lPath);
			}
		} else {
			float temperature;
			thermometer >> temperature;
			std::cerr << lPath << " " << temperature << " " << step << "\n";
			if (-55 <= temperature || temperature <= 125) { // limits according to DS18B20 data sheet
				medianBuf.push_back(temperature);
			} else {
				std::cerr << "bad temperature " << temperature << " on " << lPath << std::endl;
			}
			if (lBadFileType != 0 && lState != lBadFileType) {
				// probably recovered from bus problems which might hint at
				// new devices connected.
				populateThermometers();
			}
		}
		if (step == 0 && medianBuf.size() > 0) {
			std::sort(medianBuf.begin(), medianBuf.end());
			valueHasChanged = fStore(medianBuf.at(medianBuf.size() / 2));
			medianBuf.clear();
		}

		return valueHasChanged;
	};
};

static void populateThermometers() {
	static std::set<std::string> knownThermometers;

	DIR *owdir = opendir("/1w/uncached");
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

class onewired: public slowcontrol::daemon {
  protected:
	void fReaderThread() override {
		auto nextHeartBeatTime = fBeatHeart();
		while (true) {
			if (lStopRequested) {
				fBeatHeart(true);
				std::cerr << "stopping reader thread" << std::endl;
				return;
			}
			std::list<owTemperature*> measurements;
			{
				std::lock_guard < decltype(lMeasurementsWithReaderMutex) > lock(lMeasurementsWithReaderMutex);
				for (auto& measurement : lMeasurementsWithDefaultReader) {
					auto temp = dynamic_cast<owTemperature*>(measurement.lBase);
					if (temp) {
						measurements.push_back(temp);
					}
				}
			}
			for (int step = 4; step >= 0; step--) {
				for (auto measurement : measurements) {
					if (fExecuteGuarded([measurement, step]() {
					measurement->fReadCurrentValueMedian(step);
					})) {
						continue;
					}
				}
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			std::this_thread::sleep_for(std::chrono::seconds(30));

			if (std::chrono::system_clock::now() > nextHeartBeatTime) {
				nextHeartBeatTime = fBeatHeart();
			}

		}
	}
  public:
	onewired(): daemon("onewired") {}
};

int main(int argc, const char *argv[]) {
	options::parser parser("slowcontrol program for reading one-wire devices via owfs");
	parser.fParse(argc, argv);



	auto daemon = new onewired();

	populateThermometers();

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
