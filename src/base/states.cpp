#include "states.h"
#include "slowcontrol.h"
#include "pgsqlWrapper.h"
std::map<std::string, measurement_state::stateType> measurement_state::gStates;
std::mutex measurement_state::gStatesMutex;

measurement_state::stateType measurement_state::fGetState(const std::string& aStateName) {
	std::lock_guard<decltype(gStatesMutex)> mapLock(gStatesMutex);
	if (gStates.empty()) { // populate the map from the database
		pgsql::request result("SELECT typename,type FROM state_types;");
		for (int i = 0; i < result.size(); i++) {
			gStates.emplace(result.getValue(i, "typename"),
			                std::stoi(result.getValue(i, "type")));
		}
	}
	auto it = gStates.find(aStateName);
	if (it == gStates.end()) {
		auto type = slowcontrol::base::fSelectOrInsert("state_types", "type",
		            "typename", aStateName.c_str());
		auto bla = gStates.emplace(aStateName, type);
		it = bla.first;
	}
	return it->second;
}

