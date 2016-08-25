#ifndef __slowcontrolDaemon_h_
#define __slowcontrolDaemon_h_

#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
class SlowcontrolMeasurementBase;

class slowcontrolDaemon {
 protected:
  std::map<int32_t,SlowcontrolMeasurementBase*> lMeasurements;
  std::vector<SlowcontrolMeasurementBase*> lMeasurementsWithDefaultReader;
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
