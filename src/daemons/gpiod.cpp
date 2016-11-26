#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "gpio.h"
#include <fstream>
#include <Options.h>





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
		auto pollMeasurement = dynamic_cast<slowcontrol::pollReaderInterface*>(new slowcontrol::gpio::input_value(it.first, it.second));
		struct pollfd pfd;
		pollMeasurement->fSetPollFd(&pfd);
		pollMeasurements.emplace(pfd.fd, pollMeasurement);
	}
	for (auto it : outPinNumbers) {
		new slowcontrol::gpio::output_value(it.first, it.second);
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
			new slowcontrol::gpio::timediff_value(inIt.first, inIt.second, outIt->second);
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
