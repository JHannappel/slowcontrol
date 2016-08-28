#ifndef __slowcontrolDaemon_h_
#define __slowcontrolDaemon_h_

#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
class SlowcontrolMeasurementBase;
class defaultReaderInterface;

class slowcontrolDaemon {
  protected:
	class defaultReadableMeasurement {
	  public:
		SlowcontrolMeasurementBase* lBase;
		defaultReaderInterface* lReader;
		defaultReadableMeasurement(decltype(lBase) aBase,
		                           decltype(lReader) aReader) :
			lBase(aBase), lReader(aReader) {};
	};
	std::map<int32_t, SlowcontrolMeasurementBase*> lMeasurements;
	std::vector<defaultReadableMeasurement> lMeasurementsWithDefaultReader;
	static slowcontrolDaemon* gInstance;
	static void fReaderThread();
	static void fStorerThread();
	static void fConfigChangeListener();
	void fDaemonize();
	std::thread* lReaderThread;
	std::thread* lStorerThread;
	std::thread* lConfigChangeListenerThread;
	std::mutex lStorerMutex;
	std::condition_variable lStorerCondition;
  public:
	slowcontrolDaemon();
	void fRegisterMeasurement(SlowcontrolMeasurementBase* aMeasurement);
	static slowcontrolDaemon* fGetInstance();
	void fStartThreads();
	void fWaitForThreads();
	void fSignalToStorer();
};


#endif
