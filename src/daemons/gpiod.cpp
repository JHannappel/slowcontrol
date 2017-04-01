#include "measurement.h"
#include "gpio.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>





int main(int argc, const char *argv[]) {
	options::parser parser("slowcontrol program for test purposes");
	options::map<unsigned int> inPinNumbers('i', "inpin", "input pin name/number pairs");
	options::map<unsigned int> outPinNumbers('o', "outpin", "output pin name/number pairs");
	options::map<unsigned int> durationInPinNumbers('I', "durationIn", "input pin name/number pairs for duration measurements");
	options::map<unsigned int> durationOutPinNumbers('O', "durationOut", "output pin name/number pairs  for duration measurements");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("gpiod");

	for (auto it : inPinNumbers) {
		new slowcontrol::gpio::input_value(it.first, it.second);
	}
	for (auto it : outPinNumbers) {
		new slowcontrol::gpio::output_value(it.first, it.second);
	}
	for (auto inIt : durationInPinNumbers) {
		auto outIt = durationOutPinNumbers.find(inIt.first);
		if (outIt != durationOutPinNumbers.end()) {
			new slowcontrol::gpio::timediff_value(inIt.first, inIt.second, outIt->second);
		}
	}


	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
