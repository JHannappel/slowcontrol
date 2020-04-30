#include "trigger.h"
#include "pgsqlWrapper.h"

namespace slowcontrol {
	trigger::trigger() {
	}
	const char *trigger::fGetDefaultTableName() const {
		return "measurements_trigger";
	}
	void trigger::fFlush(bool /* aFlushSingleValue */) {
	}
	void trigger::fSendValues() {
		while (! lSendQueue.empty()) { // empty() is thread safe by itself
			timeType value;
			{
				// scope for send queue locking
				std::lock_guard<std::mutex> SendQueueLock(lSendQueueMutex);
				value = lSendQueue.front();
				lSendQueue.pop_front();
			}
			std::string  query("INSERT INTO ");
			query += fGetDefaultTableName();
			query += " (uid,time) VALUES (";
			query += std::to_string(fGetUid());
			query += ", (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
			query += std::to_string(std::chrono::duration<double, std::nano>(value.time_since_epoch()).count() / 1E9);
			query += " * INTERVAL '1 second'));";

			pgsql::request{query};
		}
	}
	bool trigger::fValuesToSend() {
		return ! lSendQueue.empty();
	}
	void trigger::fTrigger(timeType aTime) { ///< register trigger event in database
		std::lock_guard<decltype(lSendQueueMutex)> SendQueueLock(lSendQueueMutex);
		lSendQueue.push_back(aTime);
		daemon::fGetInstance()->fSignalToStorer();
	}
} // namespace slowcontrol
