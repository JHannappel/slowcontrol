#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "gpio.h"
#include <iostream>
#include <Options.h>
#include <deque>
template <typename T> class messageQueue {
  protected:
	std::deque <T> lQueue;
	std::mutex lMutex;
	std::condition_variable lWaitCondition;
  public:
	typedef T messageType;
	void fEnqueue(const T& aItem) {
		{
			std::lock_guard<decltype(lMutex)> lock(lMutex);
			lQueue.emplace_back(aItem);
		}
		lWaitCondition.notify_one();
	};
	bool fDequeue(T& aItem) {
		std::unique_lock<decltype(lMutex)> lock(lMutex);
		if (lWaitCondition.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout) {
			return false;
		};
		if (lQueue.empty()) {
			return false;
		}
		aItem = lQueue.front();
		lQueue.pop_front();
		return true;
	};
};



class cameraRecording: public slowcontrol::measurement<bool> {
  protected:
	slowcontrol::configValue<unsigned int> lChunkLength;
	slowcontrol::configValue<float> lMaxUnenlightenedDarkness;
	slowcontrol::configValue<unsigned int> lFramesPerSecond;
	slowcontrol::configValue<unsigned int> lWidth;
	slowcontrol::configValue<unsigned int> lHeight;
	slowcontrol::configValue<std::string> lFilePattern;
	slowcontrol::configValue<std::string> lTmpDir;
	slowcontrol::configValue<std::string> lOutputDir;
	std::string lCommand;
	std::string lPostProcessCommand;

	messageQueue<std::string> lMessageQueue;
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
		lCommand += lTmpDir;
		lPostProcessCommand = "MP4Box -fps ";
		lPostProcessCommand += std::to_string(lFramesPerSecond);
		lPostProcessCommand += " -add ";
		lPostProcessCommand += lTmpDir;
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
		lFilePattern("filePattern", lConfigValues, "%Y%m%d_%H%M%S"),
		lTmpDir("tmpDir", lConfigValues, "/run/"), // that's a ramdisk
		lOutputDir("outputDir", lConfigValues, "/video/") {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
		slowcontrol::daemon::fGetInstance()->fAddThread(new std::thread(&cameraRecording::fPostProcess, this));
	}
	void fRecord() {
		char filename[1024];
		std::time_t now = std::time(NULL);
		std::strftime(filename, sizeof(filename), lFilePattern.fGetValue().c_str(), std::localtime(&now));
		std::string command(lCommand);
		command += filename;
		command += ".h264";
		system(command.c_str());
		lMessageQueue.fEnqueue(filename);
	}
	void fPostProcess() {
		while (!slowcontrol::daemon::fGetInstance()->fGetStopRequested()) {
			std::string filename;
			if (lMessageQueue.fDequeue(filename)) {
				std::string command(lPostProcessCommand);
				command += filename;
				command += ".h264 ";
				command += lOutputDir;
				command += filename;
				command += ".mp4";
				system(command.c_str());
				command = "/run/";
				command += filename;
				command += ".h264";
				unlink(command.c_str());
			}
		}
		std::cout << "stopping post process thread\n";
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

	slowcontrol::watch_pack watchPack;
	slowcontrol::watched_measurement<slowcontrol::gpio::input_value>  motionDet(watchPack,
	[](slowcontrol::gpio::input_value * aThat) {
		return aThat->fGetCurrentValue();
	},
	name.fGetValue() + "_motion", motionPin);
	slowcontrol::gpio::output_value lightSwitch(name.fGetValue() + "_light", lightPin);
	slowcontrol::gpio::timediff_value darknessDet(name.fGetValue() + "_darkness", darknessInPin, darknessOutPin);

	daemon->fStartThreads();

	camera.fStore(false);
	while (!daemon->fGetStopRequested()) {
		if (watchPack.fWaitForChange()) {
			auto darkness = darknessDet.fGetCurrentValue();

			if (darkness > camera.fGetMaxUnenlightenedDarkness()) { // dark, switch on the light
				lightSwitch.fSet(true);
			}
			camera.fStore(true);
			while (motionDet.fGetCurrentValue() && !daemon->fGetStopRequested()) {
				camera.fRecord();
			}
			camera.fStore(false);
			if (darkness > camera.fGetMaxUnenlightenedDarkness()) { // dark, switch off the light
				lightSwitch.fSet(false);
			}
		}
	}

	daemon->fWaitForThreads();
}
