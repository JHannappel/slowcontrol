#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>


class testvalue: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<short>>,
	        public slowcontrol::writeValueWithType<short> {
  public:
	testvalue(const char *aName):
		boundCheckerInterface(0, 0, 100) {
		fInitializeUid(aName);
		fConfigure();
	}
	virtual bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			fStore(req->lGoalValue);
			aResponse = "done.";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	};
};


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for test purposes");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("testd");

	testvalue test_read("test");
	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
