#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <stdlib.h>

#include <Options.h>
class presence: public slowcontrol::measurement<bool>,
	public slowcontrol::defaultReaderInterface {
  protected:
	slowcontrol::configValue<measurementBase::durationType> lQuickReadoutInterval;
	std::string lCommand;
  public:
	presence(const std::string& aAddress):
		measurement(),
		defaultReaderInterface(lConfigValues, std::chrono::minutes(10)),
		lQuickReadoutInterval("quickReadoutInterval", lConfigValues, std::chrono::seconds(10)) {
		lClassName.fSetFromString(__func__);
		lCommand = "l2ping -c 1 ";
		lCommand += aAddress;
		std::string description(aAddress);
		description += " presence";
		fInitializeUid(description);
		fConfigure();
	};
	virtual bool fReadCurrentValue() {
		if (system(lCommand.c_str()) == 0) {
			return fStore(true);
		} else {
			return fStore(false);
		}
	};
	virtual decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const {
		if (lNoValueYet || lOldValue) {
			return lReadoutInterval.fGetValue();
		} else {
			return lQuickReadoutInterval.fGetValue();
		}
	}
};



int main(int argc, const char *argv[]) {
	OptionMap< std::string> btAdresses('a', "address", "bluetooth adresses");

	OptionParser parser("slowcontrol program for detecting presence of bluetooth devices");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("bluetoothPresenced");

	for (auto& addr : btAdresses) {
		new presence(addr.second);
	}

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
