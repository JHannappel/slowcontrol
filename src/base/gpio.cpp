#include "gpio.h"
#include <fstream>
#include <iostream>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
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
		bool pin_base::fRead() {
			lseek(lValueFd, 0, SEEK_SET);
			char buffer;
			if (read(lValueFd, &buffer, 1) < 1) {
				throw slowcontrol::exception("can't read gpio", slowcontrol::exception::level::kContinue);
			}
			return buffer == '1';
		};

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

		output::output(unsigned int aPinNumber) : pin_base(aPinNumber) {
			{
				std::ofstream director(lDirPath + "/direction");
				director << "out\n";
			}
			std::string valuepath(lDirPath);
			valuepath += "/value";
			lValueFd = open(valuepath.c_str(), O_RDWR);
		}

		void output::fWrite(bool aValue) {
			if (aValue) {
				if (write(lValueFd, "1\n", 2) < 2) {
					throw slowcontrol::exception("can't write to gpio", slowcontrol::exception::level::kStop);
				}
			} else {
				if (write(lValueFd, "0\n", 2) < 2) {
					throw slowcontrol::exception("can't write to gpio", slowcontrol::exception::level::kStop);
				}
			}
		}

		input_value::input_value(const std::string &aName,
		                         unsigned int aPinNumber):
			lInPin(aPinNumber),
			lReadDelay("readDelay",lConfigValues, measurementBase::durationType::zero()),	
			lInvert("invert",lConfigValues, false) {	
			lClassName.fSetFromString(__func__);
			fInitializeUid(aName);
			fConfigure();
			fStore(lInvert ? !lInPin.fRead() : lInPin.fRead());
		}
		void input_value::fSetPollFd(struct pollfd *aPollfd) {
			aPollfd->fd = lInPin.fGetFd();
			aPollfd->events = POLLPRI | POLLERR;
		}
		bool input_value:: fProcessData(short /*aRevents*/) {
			if (lReadDelay.fGetValue().count()!=0) {
				std::this_thread::sleep_for(lReadDelay.fGetValue());
			}
			return fStore(lInvert ? !lInPin.fRead() : lInPin.fRead());
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
				auto result = lOutPin.fRead();
				fStore(result);
				if (result == req->lGoalValue) {
					aResponse = "done.";
				} else {
					aResponse = "failed.";
				}
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
		bool timediff_value::fReadCurrentValue() {
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
				return fStore(std::chrono::duration_cast<std::chrono::nanoseconds>(timeDiff).count() * 1.0e-9, startTime);
			}
			return false;
		}
	} // end namespace gpio
} // end namespace slowcontrol
