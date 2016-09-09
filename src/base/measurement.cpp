#include "measurement.h"
#include "slowcontrol.h"
#include "slowcontrolDaemon.h"
#include "states.h"
#include <string.h>

namespace slowcontrol {

	measurementBase::measurementBase() {
		lState = measurement_state::fGetState("normal");
	};

	void measurementBase::fInitializeUid(const std::string& aDescription) {
		bool sendDefaultConfigValues = false;
		lUid = base::fSelectOrInsert("uid_list", "uid",
		                             "description", aDescription.c_str(),
		                             "data_table", fGetDefaultTableName(),
		                             &sendDefaultConfigValues);
		if (sendDefaultConfigValues) {
			for (auto it : lConfigValues) {
				fSaveOption(*(it.second), "initial default");
			}
		}
		if (dynamic_cast<writeValue*>(this) != nullptr) {
			std::string query("UPDATE uid_list SET is_write_value='true' WHERE uid=");
			query += std::to_string(fGetUid());
			query += ";";
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			PQclear(result);
		}
		daemon::fGetInstance()->fRegisterMeasurement(this);
	};


	void measurementBase::fSaveOption(const configValueBase& aCfgValue,
	                                  const char *aComment) {
		std::string query("INSERT INTO uid_configs (uid,name,value,comment) VALUES (");
		query += std::to_string(fGetUid());
		query += ",";
		base::fAddEscapedStringToQuery(aCfgValue.fGetName(), query);
		query += ",";
		std::string valueRaw;
		aCfgValue.fAsString(valueRaw);
		base::fAddEscapedStringToQuery(valueRaw, query);
		query += ",";
		base::fAddEscapedStringToQuery(aComment, query);
		query += ");";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		PQclear(result);
	}

	void measurementBase::fConfigure() {
		std::string query("SELECT name,value,comment FROM uid_configs WHERE uid=");
		query += std::to_string(fGetUid());
		query += ";";
		std::set<std::string> optionsInDb;
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		for (int i = 0; i < PQntuples(result); ++i) {
			std::string name(PQgetvalue(result, i, PQfnumber(result, "name")));
			auto it = lConfigValues.find(name);
			if (it != lConfigValues.end()) {
				auto cfgVal = it->second;
				std::string valueRaw;
				auto valueInDb = PQgetvalue(result, i, PQfnumber(result, "value"));
				cfgVal->fAsString(valueRaw);
				std::string comment(PQgetvalue(result, i, PQfnumber(result, "comment")));
				if (comment.rfind("default") == comment.size() - 7
				        && valueRaw.compare(valueInDb) != 0) {
					query = "UPDATE uid_configs SET value=";
					base::fAddEscapedStringToQuery(valueRaw, query);
					query += ", comment='changed default' WHERE uid=";
					query += std::to_string(fGetUid());
					query += " AND name=";
					base::fAddEscapedStringToQuery(name, query);
					query += ";";
					auto result2 =  PQexec(base::fGetDbconn(), query.c_str());
					PQclear(result2);
				} else {
					cfgVal->fSetFromString(valueInDb);
				}
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

	measurement_state::stateType measurementBase::fSetState(const std::string& aStateName,
	        const std::string& aReason) {
		auto newState = measurement_state::fGetState(aStateName);
		if (newState != lState) {
			std::string query("UPDATE uid_states SET type =");
			query += std::to_string(newState);
			query += ", valid_from = now(), reason=";
			base::fAddEscapedStringToQuery(aReason, query);
			query += " WHERE uid=";
			query += std::to_string(fGetUid());
			query += ";";
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			PQclear(result);
			lState = newState;
		}
		return newState;
	}

} // end of namespace slowcontrol
