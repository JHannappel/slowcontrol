#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



class gpiopin_base {
  protected:
	unsigned int lPinNumber;
	std::string lDirPath;
	int lValueFd;
  public:
	gpiopin_base(unsigned int aPinNumber):
		lPinNumber(aPinNumber) {
		std::ofstream exporter("/sys/class/gpio/export");
		exporter << aPinNumber << "\n";
		lDirPath = "/sys/class/gpio/gpio";
		lDirPath += std::to_string(lPinNumber);
	};
	int fGetFd() {
		return lValueFd;
	};
};

class gpio_input: public gpiopin_base {
  public:
	gpio_input(unsigned int aPinNumber) : gpiopin_base(aPinNumber) {
		{
			std::ofstream director(lDirPath + "/direction");
			director << "in\n";
		}
		{
			std::ofstream director(lDirPath + "/edge");
			director << "both\n";
		}
		std::string valuepath(lDirPath);
		valuepath += "/value";
		lValueFd = open(valuepath.c_str(), O_RDONLY);
	};
	bool fRead() {
		lseek(lValueFd, 0, SEEK_SET);
		char buffer;
		read(lValueFd, &buffer, 1);
		return buffer == '1';
	};
};
class gpio_output: public gpiopin_base {
  public:
	gpio_output(unsigned int aPinNumber) : gpiopin_base(aPinNumber) {
		{
			std::ofstream director(lDirPath + "/direction");
			director << "out\n";
		}
		std::string valuepath(lDirPath);
		valuepath += "/value";
		lValueFd = open(valuepath.c_str(), O_WRONLY);
	};
	void fWrite(bool aValue) {
		if (aValue) {
			write(lValueFd, "1\n", 2);
		} else {
			write(lValueFd, "0\n", 2);
		}
	}
};


class gpio_input_value: public slowcontrol::measurement<bool>,
	public slowcontrol::pollReaderInterface,
	public gpio_input {
  public:
	gpio_input_value(const std::string &aName,
	                 unsigned int aPinNumber):
		gpio_input(aPinNumber) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	};
	virtual void fSetPollFd(struct pollfd *aPollfd) {
		aPollfd->fd = lValueFd;
		aPollfd->events = POLLPRI | POLLERR;
	};
	virtual void fProcessData(short /*aRevents*/) {
		fStore(fRead());
	};

};
class gpio_output_value: public slowcontrol::measurement<bool>,
	public slowcontrol::writeValueWithType<bool>,
	public gpio_output {
  public:
	gpio_output_value(const std::string &aName,
	                  unsigned int aPinNumber):
		gpio_output(aPinNumber) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	}

	virtual bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			fWrite(req->lGoalValue);
			fStore(req->lGoalValue);
			aResponse = "done.";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	};
};

class gpio_timediff_value: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<float>>,
	        public slowcontrol::defaultReaderInterface {
  protected:
	gpio_input lInPin;
	gpio_output lOutPin;
  public:
	gpio_timediff_value(const std::string& aName, unsigned int aInPin, unsigned int aOutPin):
		boundCheckerInterface(0.5, 0, 125),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		lInPin(aInPin),
		lOutPin(aOutPin) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(aName);
		fConfigure();
	};
	virtual void fReadCurrentValue() {
		struct pollfd pfd;
		pfd.fd = lInPin.fGetFd();
		pfd.events = POLLPRI | POLLERR;
		lOutPin.fWrite(true);
		while (lInPin.fRead() == false) {
			poll(&pfd, 1, 1000);
		}
		lOutPin.fWrite(false);
		auto startTime = std::chrono::system_clock::now();
		if (poll(&pfd, 1, 1000) > 0) {
			auto stopTime = std::chrono::system_clock::now();
			auto timeDiff = stopTime - startTime;
			fStore(timeDiff.count(), startTime);
		}
	}
};

int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for test purposes");
	OptionMap<unsigned int> inPinNumbers('i', "inpin", "input pin name/number pairs");
	OptionMap<unsigned int> outPinNumbers('o', "outpin", "output pin name/number pairs");
	OptionMap<unsigned int> durationInPinNumbers('I', "durationIn", "input pin name/number pairs for duration measurements");
	OptionMap<unsigned int> durationOutPinNumbers('O', "durationOut", "output pin name/number pairs  for durationOut measurements");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("gpiod");
	std::map<int, slowcontrol::pollReaderInterface*> pollMeasurements;

	for (auto it : inPinNumbers) {
		auto pollMeasurement = dynamic_cast<slowcontrol::pollReaderInterface*>(new gpio_input_value(it.first, it.second));
		struct pollfd pfd;
		pollMeasurement->fSetPollFd(&pfd);
		pollMeasurements.emplace(pfd.fd, pollMeasurement);
	}
	for (auto it : outPinNumbers) {
		new gpio_output_value(it.first, it.second);
	}
	std::vector<struct pollfd> pfds;
	for (auto pollMeasurement : pollMeasurements) {
		struct pollfd pfd;
		pollMeasurement.second->fSetPollFd(&pfd);
		pfds.emplace_back(pfd);
	}
	for (auto inIt : durationInPinNumbers) {
		auto outIt = durationOutPinNumbers.find(inIt.first);
		if (outIt != durationOutPinNumbers.end()) {
			new gpio_timediff_value(inIt.first, inIt.second, outIt->second);
		}
	}


	daemon->fStartThreads();

	while (!daemon->fGetStopRequested()) {
		auto result = poll(pfds.data(), pfds.size(), 1000);
		if (result > 0) {
			for (auto& pfd : pfds) {
				if (pfd.revents != 0) {
					auto it = pollMeasurements.find(pfd.fd);
					if (it != pollMeasurements.end()) {
						it->second->fProcessData(pfd.revents);
					}
				}
			}
		}
	}

	daemon->fWaitForThreads();
}
