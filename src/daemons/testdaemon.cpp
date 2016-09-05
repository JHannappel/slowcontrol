#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>


class testvalue: public slowcontrol::boundCheckerInterface<slowcontrol::measurement<short>>,
	        public slowcontrol::writeValueInterface {
  public:
	testvalue(const char *aName):
		boundCheckerInterface(0, 0, 100) {
		fInitializeUid(aName);
		fConfigure();
	}
	virtual bool fProcessRequest(const std::string& aRequest, std::string& aResponse) {
		short value;
		if (fParseForSet(aRequest, value)) {
			std::cerr << "set value to " << value << std::endl;
			fStore(value);
			aResponse= "done.";
			return true;
		}
		aResponse="bad request.";
		return false;
	}
};


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for test purposes");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrol::daemon("testd");

	testvalue test_read("test");
	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
