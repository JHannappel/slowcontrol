#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "communications.h"

#include <Options.h>
class koradPowerSupply;

class koradValue : public slowcontrol::measurement<float> {
  protected:
	koradPowerSupply& lSupply;
  public:
	koradValue(koradPowerSupply& aSupply,
	           const std::string& aPsName,
	           const std::string& aValueName	     ):
		measurement(0.001),
		lSupply(aSupply) {
		std::string name(aPsName);
		name += "_";
		name += aValueName;

	};
};
class koradSetValue: public koradValue,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::writeValueWithType<float>,
	public slowcontrol::unitInterface {
  protected:
	std::string lSetCommandFormat;
	std::string lReadBackCommand;
  public:
	koradSetValue(koradPowerSupply& aSupply,
	              const std::string& aPsName,
	              const std::string& aValueName,
	              const std::string& aSetCommandFormat,
	              const std::string& aReadBackCommand,
	              const char *aUnit):
		koradValue(aSupply, aPsName, aValueName),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(10)),
		unitInterface(lConfigValues, aUnit),
		lSetCommandFormat(aSetCommandFormat),
		lReadBackCommand(aReadBackCommand) {
	};
	virtual bool fProcessRequest(const request* aRequest, std::string& aResponse);
	virtual void fReadCurrentValue();
};



class koradPowerSupply {
  protected:
	slowcontrol::serialLine lSerial;
	koradSetValue lVSet;
	koradSetValue lISet;
  public:
	koradPowerSupply(const std::string& aName,
	                 const std::string& aDevice) :
		lSerial(aDevice, 9600),
		lVSet(*this, aName, aDevice, "VSET1:%05.2f", "VSET1?", "V"),
		lISet(*this, aName, aDevice, "ISET1:%05.2f", "ISET1?", "A") {

	};
	slowcontrol::serialLine* fGetSerialLine() {
		return &lSerial;
	};
};

bool koradSetValue::fProcessRequest(const request* aRequest, std::string& aResponse) {
	auto req = dynamic_cast<const requestWithType*>(aRequest);
	if (req != nullptr) {
		char buffer[128];
		sprintf(buffer, lSetCommandFormat.c_str(), req->lGoalValue);
		lSupply.fGetSerialLine()->fWrite(buffer);
		fReadCurrentValue();
		if (fGetCurrentValue() == req->lGoalValue) {
			aResponse = "done.";
		} else {
			aResponse = "failed, got ";
			aResponse += std::to_string(fGetCurrentValue());
		}
		return true;
	}
	aResponse = "can't cast request";
	return false;
}
void koradSetValue::fReadCurrentValue() {
	lSupply.fGetSerialLine()->fWrite(lReadBackCommand.c_str());
	char buffer[16];
	lSupply.fGetSerialLine()->fRead(buffer, 5, std::chrono::seconds(2));
	fStore(std::stof(buffer));
}



int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program controlling korad power supplies");
	OptionMap<std::string> supplyDevices('d', "device", "power supply name/device pairs");
	parser.fParse(argc, argv);
	auto daemon = new slowcontrol::daemon("korad_d");

	for (auto it : supplyDevices) {
		new koradPowerSupply(it.first, it.second);
	}

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
