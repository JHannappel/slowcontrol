#ifndef __slowcontrolDaemon_h_
#define __slowcontrolDaemon_h_

#include <map>
#include <set>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <slowcontrol.h>

namespace slowcontrol {

	class heartBeatSkew;
	class measurementBase;
	class defaultReaderInterface;
	class pollReaderInterface;
	class writeValue;
	class daemon {
		typedef int32_t daemonIdType;
	  protected:
		class defaultReadableMeasurement {
		  public:
			measurementBase* lBase;
			defaultReaderInterface* lReader;
			defaultReadableMeasurement(decltype(lBase) aBase,
			                           decltype(lReader) aReader) :
				lBase(aBase), lReader(aReader) {};
		};
		class writeableMeasurement {
		  public:
			measurementBase* lBase;
			writeValue* lWriter;
			writeableMeasurement(decltype(lBase) aBase,
			                     decltype(lWriter) aWriter) :
				lBase(aBase), lWriter(aWriter) {};
		};
		std::vector<measurementBase*> lMeasurements;
		std::mutex lMeasurementsMutex;
		std::vector<defaultReadableMeasurement> lMeasurementsWithDefaultReader;
		std::mutex lMeasurementsWithReaderMutex;
		std::map<int, pollReaderInterface*> lMeasurementsWithPollReader;
		std::mutex lMeasurementsWithPollReaderMutex;
		std::map<base::uidType, writeableMeasurement> lWriteableMeasurements;
		std::mutex lMeasurementsWriteableMutex;

		std::mutex lScheduledWriteRequestMutex;
		std::condition_variable lScheduledWriteRequestWaitCondition;

		daemonIdType lId;
		std::atomic<bool> lStopRequested;
		std::mutex lWaitConditionMutex;
		std::condition_variable lWaitCondition;
		std::chrono::system_clock::duration lHeartBeatPeriod;
		heartBeatSkew* lHeartBeatSkew;
		static daemon* gInstance;
		void fSignalCatcherThread();
		void fReaderThread();
		void fPollerThread();
		void fScheduledWriterThread();
		void fStorerThread();
		void fConfigChangeListener();
		void fDaemonize();
		std::chrono::system_clock::time_point fBeatHeart(bool aLastTime = false);
		std::set<std::thread*, bool (*)(std::thread*, std::thread *)> lThreads;
		std::mutex lStorerMutex;
		std::condition_variable lStorerCondition;
		void fFlushAllValues();
		void fClearOldPendingRequests();
	  public:
		daemon(const char *aName);
		void fRegisterMeasurement(measurementBase* aMeasurement);
		static daemon* fGetInstance();
		void fStartThreads();
		void fWaitForThreads();
		void fAddThread(std::thread *aThread);
		void fSignalToStorer();
		bool fGetStopRequested() const {
			return lStopRequested;
		};
		void fProcessPendingRequests();
		template <class Clock, class Duration> void fWaitUntil(const std::chrono::time_point<Clock, Duration>& aWhen) {
			std::unique_lock<std::mutex> lock(lWaitConditionMutex);
			lWaitCondition.wait_until(lock, aWhen);
		};
		template <class Duration> void fWaitFor(Duration aDuration) {
			auto then = std::chrono::steady_clock::now() + aDuration;
			fWaitUntil(then);
		};
	};

} // end of namespace slowcontrol

#endif
