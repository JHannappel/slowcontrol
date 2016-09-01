#include "measurement.h"
#include "slowcontrol.h"
#include "slowcontrolDaemon.h"
#include "states.h"
#include <string.h>

SlowcontrolMeasurementBase::SlowcontrolMeasurementBase(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
        decltype(lReadoutInterval.fGetValue()) aDefaultReadoutIterval):
	lMaxDeltaT("MaxDeltaT", lConfigValues, aDefaultMaxDeltat),
	lReadoutInterval("ReadoutInterval", lConfigValues, aDefaultReadoutIterval) {
	lState = measurement_state::fGetState("normal");
};

void SlowcontrolMeasurementBase::fInitializeUid(const std::string& aDescription) {
	bool sendDefaultConfigValues = false;
	lUid = slowcontrol::fSelectOrInsert("uid_list", "uid",
	                                    "description", aDescription.c_str(),
	                                    "data_table", fGetDefaultTableName(),
	                                    &sendDefaultConfigValues);
	if (sendDefaultConfigValues) {
		for (auto it : lConfigValues) {
			fSaveOption(*(it.second), "initial default");
		}
	}

	slowcontrolDaemon::fGetInstance()->fRegisterMeasurement(this);
};


void SlowcontrolMeasurementBase::fSaveOption(const configValueBase& aCfgValue,
        const char *aComment) {
	std::string query("INSERT INTO uid_configs (uid,name,value,comment) VALUES (");
	query += std::to_string(fGetUid());
	query += ",";
	slowcontrol::fAddEscapedStringToQuery(aCfgValue.fGetName(), query);
	query += ",";
	std::string valueRaw;
	aCfgValue.fAsString(valueRaw);
	slowcontrol::fAddEscapedStringToQuery(valueRaw, query);
	query += ",";
	slowcontrol::fAddEscapedStringToQuery(aComment, query);
	query += ");";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	PQclear(result);
}

void SlowcontrolMeasurementBase::fConfigure() {
	std::string query("SELECT name,value FROM uid_configs WHERE uid=");
	query += std::to_string(fGetUid());
	query += ";";
	std::set<std::string> optionsInDb;
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	for (int i = 0; i < PQntuples(result); ++i) {
		std::string name(PQgetvalue(result, i, PQfnumber(result, "name")));
		auto it = lConfigValues.find(name);
		if (it != lConfigValues.end()) {
			auto cfgVal = it->second;
			cfgVal->fSetFromString(PQgetvalue(result, i, PQfnumber(result, "value")));
			optionsInDb.emplace(name);
		} else {
			std::cerr << "unknown cfg option '" << name << "' with value '" << PQgetvalue(result, i, PQfnumber(result, "value")) << "' encountered for uid " << fGetUid() << std::endl;
		}
	}
	PQclear(result);
	for (auto it : lConfigValues) {
		auto name = it.first;
		if (optionsInDb.find(name) == optionsInDb.end()) {
			fSaveOption(*(it.second), "later default");
		}
	}
};

measurement_state::stateType SlowcontrolMeasurementBase::fSetState(const std::string& aStateName,
        const std::string& aReason) {
	auto newState = measurement_state::fGetState(aStateName);
	if (newState != lState) {
		std::string query("UPDATE uid_states SET type =");
		query += std::to_string(newState);
		query += ", valid_from = now(), reason=";
		slowcontrol::fAddEscapedStringToQuery(aReason, query);
		query += " WHERE uid=";
		query += std::to_string(fGetUid());
		query += ";";
		auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
		PQclear(result);
		lState = newState;
	}
	return newState;
}


