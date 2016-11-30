#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "gpio.h"
#include <fstream>
#include <Options.h>


class cameraRecording: public slowcontrol::measurement<bool> {
  protected:
	configValue<unsigned int> lChunkLength;
	configValue<float> lMaxUnenlightenedDarkness;
	configValue<unsigned int> lFramesPerSecond;
	configValue<unsigned int> lWidth;
	configValue<unsigned int> lHeight;
	configValue<std::string> lFilePattern;
	std::string lCommand;
  public:
	virtual void fConfigure() {
		measurement::fConfigure();
		lCommand = "raspivid -t ";
		lCommand += std::to_string(lChunkLength);
		lCommand += " -fps ";
		lCommand += std::to_string(lFramesPerSecond);
		lCommand += " -w ";
		lCommand += std::to_string(lWidth);
		lCommand += " -h ";
		lCommand += std::to_string(lHeight);
		lCommand += " -ae 10,0x00,0x8080FF -a 4 -a '%Y-%m-%d %H:%M:%S' ";
		lCommand += " -o ";
		lCommand += lFilePattern;
	}
	float fGetMaxUnenlightenedDarkness() {
		return lMaxUnenlightenedDarkness;
	};
	const char *fGetCommand() {
		return lCommand.c_str();
	}
	cameraRecording(const std::string& aName):
		lChunkLength("chunkLength", lConfigValues, 30000),
		lMaxUnenlightenedDarkness("MaxUnenlightenedDarkness", lConfigValues, 0.05),
		lFramesPerSecond("framesPerSecond", lConfigValues, 25),
		lWidth("with", lConfigValues, 1296),
		lHeight("height", lConfigValues, 972),
		lFilePattern("filePattern", lConfigValues, "/tmp/vid/$(date +%Y%m%d_%H%M%S).h264" ) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	}
};

int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for test purposes");
	Option<std::string> name('n', "name", "camera and support base name");
	Option<unsigned int> lightPin('l', "lightPin", "gpiopin to switch on light", 17);
	Option<unsigned int> darknessInPin('d', "darkIn", "gpiopin for darkness det (in)", 22);
	Option<unsigned int> darknessOutPin('D', "darkOut", "gpiopin for darkness det (out)", 27);
	Option<unsigned int> motionPin('m', "motionPin", "gpiopin for motion detector", 18);

	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("raspicamd");

	cameraRecording camera(name);

	//	motionWatch motionDet(name.fGetValue() + "_motion" , motionPin);
	slowcontrol::watched_measurement<slowcontrol::gpio::input_value>  motionDet([](slowcontrol::gpio::input_value * aThat) {
		return aThat->fGetCurrentValue();
	}, name.fGetValue() + "_motion" , motionPin);
	slowcontrol::gpio::output_value lightSwitch(name.fGetValue() + "_light", lightPin);
	slowcontrol::gpio::timediff_value darknessDet(name.fGetValue() + "_darkness", darknessInPin, darknessOutPin);

	daemon->fStartThreads();

	while (!daemon->fGetStopRequested()) {
		if (motionDet.fWaitForChange()) {
			auto darkness = darknessDet.fGetCurrentValue();

			if (darkness > camera.fGetMaxUnenlightenedDarkness()) { // dark, switch on the light
				lightSwitch.fSet(true);
			}
			camera.fStore(true);
			while (motionDet.fGetCurrentValue()) {
				system(camera.fGetCommand());
			}
			camera.fStore(false);
			if (darkness > camera.fGetMaxUnenlightenedDarkness()) { // dark, switch off the light
				lightSwitch.fSet(false);
			}
		}
	}

	daemon->fWaitForThreads();
}
