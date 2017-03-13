#include "measurement.h"
#include "slowcontrol.h"
#include "slowcontrolDaemon.h"
#include "states.h"
#include <string.h>

namespace slowcontrol {

	measurementBase::measurementBase():
		lClassName("className", lConfigValues) {
		lState = measurement_state::fGetState("normal");
	};

	void measurementBase::fInitializeUid(const std::string& aDescription) {
		bool sendDefaultConfigValues = false;
		lUid = base::fSelectOrInsert("uid_list", "uid",
		                             "description", aDescription.c_str(),
		                             "data_table", fGetDefaultTableName(),
		                             &sendDefaultConfigValues);
		if (sendDefaultConfigValues) { // we must initialize some other table entries
			for (auto it : lConfigValues) {
				fSaveOption(*(it.second), "initial default");
			}

			// the insert into the uid_states table can't be done via rule in the databse,
			// because the new.uid is not the freshly inserted one but the next value
			std::string  query("INSERT INTO uid_states (uid,type,valid_from,reason) VALUES (");
			query += std::to_string(fGetUid());
			query += ",";
			query += std::to_string(lState);
			query += ",'-infinity','initial state');";
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			PQclear(result);
		} else {
			std::string  query("SELECT type FROM uid_states WHERE uid=");
			query += std::to_string(fGetUid());
			query += ";";
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			if (PQntuples(result) < 1) {
				PQclear(result);
				query = "INSERT INTO uid_states (uid,type,valid_from,reason) VALUES (";
				query += std::to_string(fGetUid());
				query += ",";
				query += std::to_string(lState);
				query += ",'-infinity','initial state');";
				result = PQexec(base::fGetDbconn(), query.c_str());
			} else {
				lState = std::stol(PQgetvalue(result, 0, 0));
			}
			PQclear(result);
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
		aCfgValue.fSave("uid_configs", "uid", fGetUid(), aComment);
	}

	void measurementBase::fUpdateOption(const configValueBase& aCfgValue,
	                                    const char *aComment) {
		aCfgValue.fUpdate("uid_configs", "uid", fGetUid(), aComment);
	}

	void measurementBase::fConfigure() {
		configValueBase::fConfigure("uid_configs", "uid", fGetUid(), lConfigValues);
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
