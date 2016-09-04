#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
class testvalue: public boundCheckerInterface<SlowcontrolMeasurement<short>>,
	        public writeValueInterface {
  public:
	testvalue(const char *aName):
		boundCheckerInterface(0, 0, 100) {
		fInitializeUid(aName);
		fConfigure();
	}
	virtual const std::string fProcessRequest(const std::string& aRequest) {
		short value;
		if (fParseForSet(aRequest, value)) {
			std::cerr << "set value to " << value << std::endl;
			fStore(value);
			return "done.";
		}
		return "bad request.";
	}
};


int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for test purposes");
	parser.fParse(argc, argv);

	auto daemon = new slowcontrolDaemon("testd");

	testvalue test_read("test");
	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
