#include "slowcontrol.h"
#include <Options.h>

int main(int argc, const char* argv[]) {
	OptionParser parser("slowcontrol program for setting values via command line");
	auto argList = parser.fParse(argc, argv);
	int uid = 1;
	auto& request = argList.at(0);
	auto& comment = argList.at(1);
	std::string response;
	auto result = slowcontrol::base::fRequestValueSetting(uid, request, comment, response);
	std::cout << response << " " << result << std::endl;
	return result ? 0 : 1;
}
