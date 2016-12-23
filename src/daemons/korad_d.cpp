#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "communications.h"

#include <Options.h>
class koradPowerSupply;

class koradValue : public slowcontrol::measurement<float>,
	public slowcontrol::unitInterface {
  protected:
	std::string lName;
	std::string lValueName;
  public:
	koradValue(koradPowerSupply* aSupply,
	           const std::string& aPsName,
	           const std::string& aValueName,
	           const char *aUnit);
	void fInit() {
		fInitializeUid(lName);
		fConfigure();
	};
	const std::string& fGetValueName() const {
		return lValueName;
	}
};
class koradReadValue: public koradValue,
	public slowcontrol::defaultReaderInterface {
  protected:
	koradPowerSupply* lSupply;
	std::string lReadBackCommand;
  public:
	koradReadValue(koradPowerSupply* aSupply,
	               const std::string& aPsName,
	               const std::string& aValueName,
	               const std::string& aReadBackCommand,
	               const char *aUnit):
		koradValue(aSupply, aPsName, aValueName, aUnit),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(10)),
		lSupply(aSupply),
		lReadBackCommand(aReadBackCommand) {
	};
	virtual bool fReadCurrentValue();
};

class koradSetValue: public koradReadValue,
	public slowcontrol::writeValueWithType<float> {
  protected:
	std::string lSetCommandFormat;
  public:
	koradSetValue(koradPowerSupply* aSupply,
	              const std::string& aPsName,
	              const std::string& aValueName,
	              const std::string& aSetCommandFormat,
	              const std::string& aReadBackCommand,
	              const char *aUnit):
		koradReadValue(aSupply, aPsName, aValueName, aReadBackCommand, aUnit),
		lSetCommandFormat(aSetCommandFormat) {
	};
	virtual bool fProcessRequest(const request* aRequest, std::string& aResponse);
};


class koradPowerSupply {
  protected:
	slowcontrol::serialLine lSerial;
	std::mutex lSerialLineMutex;
	std::vector<koradValue*> lValues;
	koradSetValue lVSet;
	koradSetValue lISet;
  public:
	slowcontrol::watched_measurement<koradReadValue> lVRead;
	slowcontrol::watched_measurement<koradReadValue> lIRead;
	koradValue lPower;
	koradValue lLoad;
	koradPowerSupply(const std::string& aName,
	                 const std::string& aDevice,
	                 slowcontrol::watch_pack& aWatchPack) :
		lSerial(aDevice, 9600),
		lVSet(this, aName, "VSet", "VSET1:%05.2f", "VSET1?", "V"),
		lISet(this, aName, "ISet", "ISET1:%05.3f", "ISET1?", "A"),
		lVRead(aWatchPack, [](koradReadValue*) -> bool {return true;}, this, aName, "VRead", "VOUT1?", "V"),
		lIRead(aWatchPack, [](koradReadValue*) -> bool {return true;}, this, aName, "IRead", "IOUT1?", "A"),
		lPower(this, aName, "Power", "W"),
		lLoad(this, aName, "Load", "Ohm") {
		auto compound = slowcontrol::base::fGetCompoundId(aName.c_str(), aName.c_str());
		for (auto value : lValues) {
			value->fInit();
			slowcontrol::base::fAddToCompound(compound, value->fGetUid(), value->fGetValueName());
		}
	};

	void fRegister(koradValue *aValue) {
		lValues.emplace_back(aValue);
	};

	void fUseSerialLine(const std::function<void(slowcontrol::serialLine&)> &aLineUser) {
		// lock the serial line mutex to avoid interference from other threads
		std::lock_guard<decltype(lSerialLineMutex)> lock(lSerialLineMutex);
		// wait until the last command is completely sent and allow for 50ms troedel time
		std::this_thread::sleep_until(lSerial.fGetLastCommunicationTime() + std::chrono::milliseconds(50));
		aLineUser(lSerial);
	}
};

bool koradSetValue::fProcessRequest(const request* aRequest, std::string& aResponse) {
	auto req = dynamic_cast<const requestWithType*>(aRequest);
	if (req != nullptr) {
		char buffer[128];
		sprintf(buffer, lSetCommandFormat.c_str(), req->lGoalValue);
		lSupply->fUseSerialLine([buffer](slowcontrol::serialLine & aLine) {
			aLine.fWrite(buffer);
			std::cout << buffer << "\n";
		});
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
bool koradReadValue::fReadCurrentValue() {
	char buffer[16];
	lSupply->fUseSerialLine([&buffer, this](slowcontrol::serialLine & aLine) {
		aLine.fFlushReceiveBuffer();
		aLine.fWrite(this->lReadBackCommand.c_str());
		std::cout << this->lReadBackCommand << "\n";
		aLine.fRead(buffer, 7, std::chrono::seconds(2));
	});
	std::cout << buffer << "\n";
	return fStore(std::stof(buffer));
}

koradValue::koradValue(koradPowerSupply* aSupply,
                       const std::string& aPsName,
                       const std::string& aValueName,
                       const char *aUnit):
	measurement(0.001),
	unitInterface(lConfigValues, aUnit),
	lValueName(aValueName) {
	lClassName.fSetFromString(__func__);
	aSupply->fRegister(this);
	lName = aPsName;
	lName += "_";
	lName += aValueName;
};


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program controlling korad power supplies");
	OptionMap<std::string> supplyDevices('d', "device", "power supply name/device pairs");
	parser.fParse(argc, argv);
	auto daemon = new slowcontrol::daemon("korad_d");

	slowcontrol::watch_pack watchPack;

	std::vector<koradPowerSupply*> powerSupplies;
	for (auto it : supplyDevices) {
		powerSupplies.emplace_back(new koradPowerSupply(it.first, it.second, watchPack));
	}

	daemon->fStartThreads();
	while (!daemon->fGetStopRequested()) {
		if (watchPack.fWaitForChange()) {
			for (auto powerSupply : powerSupplies) {
				auto voltage = powerSupply->lVRead.fGetCurrentValue();
				auto current = powerSupply->lIRead.fGetCurrentValue();
				powerSupply->lPower.fStore(voltage * current);
				powerSupply->lLoad.fStore(voltage / current);
			}
		}
	}
	daemon->fWaitForThreads();
}
