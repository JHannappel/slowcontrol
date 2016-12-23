#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "communications.h"

#include <Options.h>
class koradPowerSupply;

class koradValue : public slowcontrol::measurement<float>,
	public slowcontrol::unitInterface {
  public:
	koradValue(const std::string& aPsName,
	           const std::string& aValueName,
	           const char *aUnit):
		measurement(0.001),
		unitInterface(lConfigValues, aUnit) {
		lClassName.fSetFromString(__func__);
		std::string name(aPsName);
		name += "_";
		name += aValueName;
		fInitializeUid(name);
		fConfigure();
	};
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
		koradValue(aPsName, aValueName, aUnit),
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
		lPower(aName, "Power", "W"),
		lLoad(aName, "Load", "Ohm") {
		auto compound = slowcontrol::base::fGetCompoundId(aName.c_str(), aName.c_str());
		slowcontrol::base::fAddToCompound(compound, lVSet.fGetUid(), "VSet");
		slowcontrol::base::fAddToCompound(compound, lISet.fGetUid(), "ISet");
		slowcontrol::base::fAddToCompound(compound, lVRead.fGetUid(), "VRead");
		slowcontrol::base::fAddToCompound(compound, lIRead.fGetUid(), "IRead");
		slowcontrol::base::fAddToCompound(compound, lPower.fGetUid(), "Power");
		slowcontrol::base::fAddToCompound(compound, lLoad.fGetUid(), "Load");
	};

	void fUseSerialLine(const std::function<void(slowcontrol::serialLine&)> &aLineUser) {
		// lock the serial line mutex to avoid inteference from other threads
		std::lock_guard<decltype(lSerialLineMutex)> lock(lSerialLineMutex);
		// wait until the last command is completely sent
		std::this_thread::sleep_until(lSerial.fGetLastCommunicationTime());
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
		aLine.fWrite(this->lReadBackCommand.c_str());
		aLine.fRead(buffer, 5, std::chrono::seconds(2));
	});
	return fStore(std::stof(buffer));
}



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
	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
