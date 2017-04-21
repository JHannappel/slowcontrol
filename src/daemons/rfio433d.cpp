#include "trigger.h"
#include "communications.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
#include <string.h>

class rslTrigger: public slowcontrol::trigger {
  protected:
	unsigned int lPattern;
	std::string lName;
  public:
	rslTrigger(const std::string& aName, unsigned int aPattern):
		lPattern(aPattern),
		lName(aName) {
		lClassName.fSetFromString(__func__);
	};
	void fInit() {
		fInitializeUid(lName);
		fConfigure();
	};
	unsigned int fGetPattern() const {
		return lPattern;
	};
};
class rslSender: public rslTrigger, public slowcontrol::writeValue {
  protected:
	slowcontrol::serialLine *lSerial;
  public:
	rslSender(const std::string& aName, unsigned int aPattern,
	          slowcontrol::serialLine *aSerial):
		rslTrigger(aName, aPattern),
		lSerial(aSerial) {
	}
	writeValue::request* fParseForRequest(const std::string& /*aRequestText*/,
	                                      request::timeType aWhen,
	                                      request::idType aRequestId) override {
		return new request(this, aWhen, aRequestId);
	}
	bool fProcessRequest(const writeValue::request* /*aRequest*/, std::string& aResponse) override {
		char buffer[128];
		sprintf(buffer, "send 6 20 240 0 6 1 5 %X\n", lPattern);
		lSerial->fWrite(buffer);
		fTrigger();
		aResponse = "done.";
		return true;
	};
};

//"send 6 20 240 0 6 1 5 B52F3680" // switch off
//"send 6 20 240 0 6 1 5 B92F3680" // switch on

int main(int argc, const char *argv[]) {
	options::parser parser("slowcontrol daemon for 433MHz RF data");
	options::single<std::string> device('d', "device", "serial device with rfio433MHz-avr");
	options::map<unsigned int> rslCommands('r', "rslcommand", "rsl command name/number pairs");
	options::map<unsigned int> rslSenders('s', "sendrslcommand", "sendable rsl command name/number pairs");
	parser.fParse(argc, argv);

	slowcontrol::serialLine serial(device, 500000);
	serial.fSetRetries(0); // don't worry about timeouts

	auto daemon = new slowcontrol::daemon("rfio433d");


	std::map<unsigned int, rslTrigger*> triggers;
	for (auto it : rslCommands) {
		std::cout << std::hex << it.first << " " << it.second << "\n";
		triggers.emplace(it.second, new rslTrigger(it.first, it.second));
	}
	for (auto it : triggers) {
		std::cout << std::hex << it.first << " " << it.second->fGetPattern() << "\n";
		it.second->fInit();
	}
	for (auto it : rslSenders) {
		auto s = new rslSender(it.first, it.second, &serial);
		s->fInit();
		std::cout << "s " << std::hex << it.first << " " << s->fGetPattern() << " c " << dynamic_cast<slowcontrol::writeValue*>(s) <<  "\n";
	}

	daemon->fStartThreads();

	unsigned int last_number = 0;
	unsigned int occurences = 0;
	while (! daemon->fGetStopRequested()) {
		char buffer[512];
		if (serial.fRead(buffer, sizeof(buffer), std::chrono::seconds(1)) > 0) {
			if (strncmp(buffer, "found ", 6) == 0) {
				auto number = std::stoul(buffer + 6, nullptr , 16);
				if (number == last_number) {
					occurences++;
					if (occurences == 3) {
						auto it = triggers.find(number);
						if (it == triggers.end()) {
							std::cout << "encountered unknown rsl number " << std::hex << number << "\n";
						} else {
							it->second->fTrigger();
						}
					}
				} else {
					last_number = number;
					occurences = 1;
				}
			}
		} else {
			last_number = 0;
			occurences = 0;
		}
	}

	daemon->fWaitForThreads();
}
