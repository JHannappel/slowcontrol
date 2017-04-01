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
	bool fReadCurrentValue() override {
		if (system(lCommand.c_str()) == 0) {
			return fStore(true);
		}
		return fStore(false);

	};
	decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const override {
		if (lNoValueYet || lOldValue) {
			return lReadoutInterval.fGetValue();
		}
		return lQuickReadoutInterval.fGetValue();

	}
};



int main(int argc, const char *argv[]) {
	options::map< std::string> btAdresses('a', "address", "bluetooth adresses");

	options::parser parser("slowcontrol program for detecting presence of bluetooth devices");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("bluetoothPresenced");

	for (auto& addr : btAdresses) {
		new presence(addr.second);
	}

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
