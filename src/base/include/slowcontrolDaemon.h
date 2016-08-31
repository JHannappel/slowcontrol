#ifndef __slowcontrolDaemon_h_
#define __slowcontrolDaemon_h_

#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <slowcontrol.h>

class heartBeatSkew;
class SlowcontrolMeasurementBase;
class defaultReaderInterface;
class slowcontrolDaemon {
	typedef int32_t daemonIdType;
  protected:
	class defaultReadableMeasurement {
	  public:
		SlowcontrolMeasurementBase* lBase;
		defaultReaderInterface* lReader;
		defaultReadableMeasurement(decltype(lBase) aBase,
		                           decltype(lReader) aReader) :
			lBase(aBase), lReader(aReader) {};
	};
	std::map<slowcontrol::uidType, SlowcontrolMeasurementBase*> lMeasurements;
	std::vector<defaultReadableMeasurement> lMeasurementsWithDefaultReader;
	daemonIdType lId;
	std::chrono::system_clock::duration lHeartBeatFrequency;
	heartBeatSkew* lHeartBeatSkew;
	static slowcontrolDaemon* gInstance;
	static void fReaderThread();
	static void fStorerThread();
	static void fConfigChangeListener();
	void fDaemonize();
	std::chrono::system_clock::time_point fBeatHeart();
	std::thread* lReaderThread;
	std::thread* lStorerThread;
	std::thread* lConfigChangeListenerThread;
	std::mutex lStorerMutex;
	std::condition_variable lStorerCondition;
  public:
	slowcontrolDaemon(const char *aName);
	void fRegisterMeasurement(SlowcontrolMeasurementBase* aMeasurement);
	static slowcontrolDaemon* fGetInstance();
	void fStartThreads();
	void fWaitForThreads();
	void fSignalToStorer();
};


#endif
