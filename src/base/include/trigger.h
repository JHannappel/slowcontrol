#ifndef __SlowcontrolTrigger_h__
#define __SlowcontrolTrigger_h__

#include "measurement.h"

namespace slowcontrol {
	/// class for trigger measurements
	/// i.e. for things that happen but do not have a value associated
	class trigger: public measurementBase {
	  protected:
		virtual const char *fGetDefaultTableName() const;
	  public:
		trigger();
		virtual void fFlush(bool aFlushSingleValue = false); ///< write current value into database
		virtual void fSendValues(); ///< send values in send queue to database
		virtual bool fValuesToSend(); ///< check if there are values to send

		virtual void fTrigger() {
			fTrigger(std::chrono::system_clock::now());
		}; ///< register trigger event in database
		virtual void fTrigger(timeType aTime); ///< register trigger event in database
	};
} // namespace slowcontrol

#endif
