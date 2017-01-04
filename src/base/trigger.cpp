#include "trigger.h"

namespace slowcontrol {
	trigger::trigger() {
	}
	const char *trigger::fGetDefaultTableName() const {
		return "measurements_trigger";
	}
	void trigger::fFlush(bool /* aFlushSingleValue */) {
	}
	void trigger::fSendValues() {
	}
	bool trigger::fValuesToSend() {
		return false;
	}
	void trigger::fTrigger(timeType aTime) { ///< register trigger event in database
		std::string  query("INSERT INTO ");
		query += fGetDefaultTableName();
		query += " (uid,time) VALUES (";
		query += std::to_string(fGetUid());
		query += ", (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
		query += std::to_string(std::chrono::duration<double, std::nano>(aTime.time_since_epoch()).count() / 1E9);
		query += " * INTERVAL '1 second'));";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		PQclear(result);
	}
} // namespace slowcontrol
