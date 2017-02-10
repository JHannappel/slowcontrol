#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "communications.h"

#include <Options.h>
class koradPowerSupply;

class koradValue {
  protected:
	std::string lName;
	std::string lValueName;
  public:
	koradValue(koradPowerSupply* aSupply,
	           const std::string& aPsName,
	           const std::string& aValueName);
	virtual void fInit() = 0;
	virtual slowcontrol::base::uidType fDoGetUid() = 0;
	const std::string& fGetValueName() const {
		return lValueName;
	};
};

template <typename T> class koradTypedValue:  public slowcontrol::measurement<T>,
	public koradValue {
  public:
	koradTypedValue(koradPowerSupply* aSupply,
	                const std::string& aPsName,
	                const std::string& aValueName) :
		koradValue(aSupply, aPsName, aValueName) {
		this->lClassName.fSetFromString(__func__);
	};
	virtual void fInit() {
		this->fInitializeUid(lName);
		this->fConfigure();
	};
	virtual slowcontrol::base::uidType fDoGetUid() {
		return this->fGetUid();
	};
};

class koradReadValue: public koradTypedValue<float>,
	public slowcontrol::defaultReaderInterface,
	public slowcontrol::unitInterface {
  protected:
	koradPowerSupply* lSupply;
	std::string lReadBackCommand;
  public:
	koradReadValue(koradPowerSupply* aSupply,
	               const std::string& aPsName,
	               const std::string& aValueName,
	               const std::string& aReadBackCommand,
	               const char *aUnit):
		koradTypedValue(aSupply, aPsName, aValueName),
		defaultReaderInterface(lConfigValues, std::chrono::milliseconds(200)),
		unitInterface(lConfigValues, aUnit),
		lSupply(aSupply),
		lReadBackCommand(aReadBackCommand) {
		lDeadBand.fSetValue(0.00001);
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

class koradDerivedValue: public koradTypedValue<float>,
	public slowcontrol::unitInterface {
  public:
	koradDerivedValue(koradPowerSupply* aSupply,
	                  const std::string& aPsName,
	                  const std::string& aValueName,
	                  const char *aUnit) :
		koradTypedValue(aSupply, aPsName, aValueName),
		unitInterface(lConfigValues, aUnit) {
	}
};

class koradStateValue: public koradTypedValue<bool> {
  public:
	koradStateValue(koradPowerSupply* aSupply,
	                const std::string& aPsName,
	                const std::string& aValueName):
		koradTypedValue(aSupply, aPsName, aValueName) {
	}
};

class koradOutValue: public koradTypedValue<bool>,
	public slowcontrol::writeValueWithType<bool>,
	public slowcontrol::defaultReaderInterface {
  protected:
	koradPowerSupply *lSupply;
	koradStateValue lCVMode;
  public:
	koradOutValue(koradPowerSupply* aSupply,
	              const std::string& aPsName):
		koradTypedValue(aSupply, aPsName, "Output"),
		defaultReaderInterface(lConfigValues, std::chrono::milliseconds(500)),
		lSupply(aSupply),
		lCVMode(aSupply, aPsName, "CVMode") {
	};
	virtual bool fProcessRequest(const request* aRequest, std::string& aResponse);
	virtual bool fReadCurrentValue();
};

class koradPowerSupply {
  protected:
	slowcontrol::serialLine lSerial;
	std::mutex lSerialLineMutex;
	std::vector<koradValue*> lValues;
	koradSetValue lVSet;
	koradSetValue lISet;
	koradOutValue lOutputSwitch;
  public:
	slowcontrol::watched_measurement<koradReadValue> lVRead;
	slowcontrol::watched_measurement<koradReadValue> lIRead;
	koradDerivedValue lPower;
	koradDerivedValue lLoad;
	koradPowerSupply(const std::string& aName,
	                 const std::string& aDevice,
	                 slowcontrol::watch_pack& aWatchPack) :
		lSerial(aDevice, 9600),
		lVSet(this, aName, "VSet", "VSET1:%05.2f", "VSET1?", "V"),
		lISet(this, aName, "ISet", "ISET1:%05.3f", "ISET1?", "A"),
		lOutputSwitch(this, aName),
		lVRead(aWatchPack, [](koradReadValue*) -> bool {return true;}, this, aName, "VRead", "VOUT1?", "V"),
		lIRead(aWatchPack, [](koradReadValue*) -> bool {return true;}, this, aName, "IRead", "IOUT1?", "A"),
		lPower(this, aName, "Power", "W"),
		lLoad(this, aName, "Load", "Ohm") {
		lSerial.fSetThrowLevel(slowcontrol::exception::level::kStop);
		lSerial.fSetRetries(0); // no retries, stop on communication errors
		auto compound = slowcontrol::base::fGetCompoundId(aName.c_str(), aName.c_str());
		for (auto value : lValues) {
			value->fInit();
			slowcontrol::base::fAddToCompound(compound, value->fDoGetUid(), value->fGetValueName());
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
	bool retval;
	try {
		lSupply->fUseSerialLine([&retval, this](slowcontrol::serialLine & aLine) {
			char buffer[16];
			aLine.fFlushReceiveBuffer();
			aLine.fWrite(this->lReadBackCommand.c_str());
			aLine.fRead(buffer, 7, std::chrono::seconds(2));
			retval = fStore(std::stof(buffer));
		});
	} catch (std::invalid_argument) {
		retval = false;
	}
	return retval;
}

koradValue::koradValue(koradPowerSupply* aSupply,
                       const std::string& aPsName,
                       const std::string& aValueName):
	lValueName(aValueName) {
	aSupply->fRegister(this);
	lName = aPsName;
	lName += "_";
	lName += aValueName;
}

bool koradOutValue::fProcessRequest(const request* aRequest, std::string& aResponse) {
	auto req = dynamic_cast<const requestWithType*>(aRequest);
	if (req != nullptr) {
		auto command = req->lGoalValue ? "OUT1" : "OUT0";
		lSupply->fUseSerialLine([command](slowcontrol::serialLine & aLine) {
			aLine.fWrite(command);
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
bool koradOutValue::fReadCurrentValue() {
	bool retval;
	lSupply->fUseSerialLine([&retval, this](slowcontrol::serialLine & aLine) {
		char buffer[16];
		aLine.fFlushReceiveBuffer();
		aLine.fWrite("STATUS?");
		aLine.fRead(buffer, 3, std::chrono::seconds(2));
		this->lCVMode.fStore(buffer[0] & 0x01);
		retval = this->fStore(buffer[0] & 0x40);
	});
	return retval;
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

	daemon->fStartThreads();
	while (!daemon->fGetStopRequested()) {
		if (watchPack.fWaitForChange()) {
			for (auto powerSupply : powerSupplies) {
				auto voltage = powerSupply->lVRead.fGetCurrentValue();
				auto current = powerSupply->lIRead.fGetCurrentValue();
				powerSupply->lPower.fStore(voltage * current);
				auto load = voltage / current;
				if (load >= 0 && load < 30 / 0.001) { // limits defined by hardware
					powerSupply->lLoad.fStore(load);
				}
			}
		}
	}
	daemon->fWaitForThreads();
}
