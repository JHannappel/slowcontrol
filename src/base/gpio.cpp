#include "gpio.h"
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

slowcontrol::gpio::pin_base::pin_base(unsigned int aPinNumber) :
	lPinNumber(aPinNumber) {
	std::ofstream exporter("/sys/class/gpio/export");
	exporter << aPinNumber << "\n";
	lDirPath = "/sys/class/gpio/gpio";
	lDirPath += std::to_string(lPinNumber);
}

slowcontrol::gpio::input::input(unsigned int aPinNumber) : pin_base(aPinNumber) {
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
bool slowcontrol::gpio::input::fRead() {
	lseek(lValueFd, 0, SEEK_SET);
	char buffer;
	read(lValueFd, &buffer, 1);
	return buffer == '1';
};

slowcontrol::gpio::output::output(unsigned int aPinNumber) : pin_base(aPinNumber) {
	{
		std::ofstream director(lDirPath + "/direction");
		director << "out\n";
	}
	std::string valuepath(lDirPath);
	valuepath += "/value";
	lValueFd = open(valuepath.c_str(), O_WRONLY);
}

void slowcontrol::gpio::output::fWrite(bool aValue) {
	if (aValue) {
		write(lValueFd, "1\n", 2);
	} else {
		write(lValueFd, "0\n", 2);
	}
}

slowcontrol::gpio::input_value::input_value(const std::string &aName,
        unsigned int aPinNumber):
	input(aPinNumber) {
	lClassName.fSetFromString(__func__);
	fInitializeUid(aName);
	fConfigure();
}
void slowcontrol::gpio::input_value::fSetPollFd(struct pollfd *aPollfd) {
	aPollfd->fd = lValueFd;
	aPollfd->events = POLLPRI | POLLERR;
}
void slowcontrol::gpio::input_value:: fProcessData(short /*aRevents*/) {
	fStore(fRead());
}
slowcontrol::gpio::output_value::output_value(const std::string &aName,
        unsigned int aPinNumber):
	output(aPinNumber) {
	lClassName.fSetFromString(__func__);
	fInitializeUid(aName);
	fConfigure();
}

bool slowcontrol::gpio::output_value::fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) {
	auto req = dynamic_cast<const requestWithType*>(aRequest);
	if (req != nullptr) {
		fWrite(req->lGoalValue);
		fStore(req->lGoalValue);
		aResponse = "done.";
		return true;
	}
	aResponse = "can't cast request";
	return false;
}

slowcontrol::gpio::timediff_value::timediff_value(const std::string& aName, unsigned int aInPin, unsigned int aOutPin):
	boundCheckerInterface(0.5, 0, 125),
	defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
	lInPin(aInPin),
	lOutPin(aOutPin) {
	lClassName.fSetFromString(__func__);
	fInitializeUid(aName);
	fConfigure();
}
void slowcontrol::gpio::timediff_value::fReadCurrentValue() {
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
