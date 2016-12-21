#include "gpio.h"
#include <iostream>
#include <fstream>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace slowcontrol {
	namespace gpio {

		pin_base::pin_base(unsigned int aPinNumber) :
			lPinNumber(aPinNumber) {
			std::ofstream exporter("/sys/class/gpio/export");
			exporter << aPinNumber << "\n";
			lDirPath = "/sys/class/gpio/gpio";
			lDirPath += std::to_string(lPinNumber);
		}

		input::input(unsigned int aPinNumber) : pin_base(aPinNumber) {
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
		bool input::fRead() {
			lseek(lValueFd, 0, SEEK_SET);
			char buffer;
			read(lValueFd, &buffer, 1);
			return buffer == '1';
		};

		output::output(unsigned int aPinNumber) : pin_base(aPinNumber) {
			{
				std::ofstream director(lDirPath + "/direction");
				director << "out\n";
			}
			std::string valuepath(lDirPath);
			valuepath += "/value";
			lValueFd = open(valuepath.c_str(), O_WRONLY);
		}

		void output::fWrite(bool aValue) {
			if (aValue) {
				write(lValueFd, "1\n", 2);
			} else {
				write(lValueFd, "0\n", 2);
			}
		}

		input_value::input_value(const std::string &aName,
		                         unsigned int aPinNumber):
			lInPin(aPinNumber) {
			lClassName.fSetFromString(__func__);
			fInitializeUid(aName);
			fConfigure();
			fStore(lInPin.fRead());
		}
		void input_value::fSetPollFd(struct pollfd *aPollfd) {
			aPollfd->fd = lInPin.fGetFd();
			aPollfd->events = POLLPRI | POLLERR;
		}
		void input_value:: fProcessData(short /*aRevents*/) {
			fStore(lInPin.fRead());
		}
		output_value::output_value(const std::string &aName,
		                           unsigned int aPinNumber):
			lOutPin(aPinNumber) {
			lClassName.fSetFromString(__func__);
			fInitializeUid(aName);
			fConfigure();
		}

		bool output_value::fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) {
			auto req = dynamic_cast<const requestWithType*>(aRequest);
			if (req != nullptr) {
				lOutPin.fWrite(req->lGoalValue);
				fStore(req->lGoalValue);
				aResponse = "done.";
				return true;
			}
			aResponse = "can't cast request";
			return false;
		}
		void output_value::fSet(bool aValue) {
			lOutPin.fWrite(aValue);
			fStore(aValue);
		}


		timediff_value::timediff_value(const std::string& aName, unsigned int aInPin, unsigned int aOutPin):
			boundCheckerInterface(0, 125, 0.5),
			defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
			lInPin(aInPin),
			lOutPin(aOutPin) {
			lClassName.fSetFromString(__func__);
			fInitializeUid(aName);
			fConfigure();
		}
		void timediff_value::fReadCurrentValue() {
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
				fStore(std::chrono::duration_cast<std::chrono::nanoseconds>(timeDiff).count() * 1.0e-9, startTime);
			}
		}
	} // end namespace gpio
} // end namespace slowcontrol
