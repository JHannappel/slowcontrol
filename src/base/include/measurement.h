#ifndef __SlowcontrolMeasuerement_h__
#define __SlowcontrolMeasuerement_h__

#include <vector>
#include <set>
#include <map>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iostream>

#include "configValue.h"
#include "slowcontrolDaemon.h"

class SlowcontrolMeasurementBase {
 protected:
  std::map<std::string, configValueBase*> lConfigValues;
  configValue<std::chrono::system_clock::duration> lMaxDeltaT;
  configValue<std::chrono::system_clock::duration> lReadoutInterval;
  std::mutex lSendQueueMutex;
  size_t lMinValueIndex;
  size_t lMaxValueIndex;
  int32_t lUid;
  virtual const char *fGetDefaultTableName() const = 0;
  void fSaveOption(const configValueBase& aCfgValue,
		   const char *comment);
  virtual void fInitializeUid(const std::string& aDescription);
 public:
  SlowcontrolMeasurementBase();
  virtual bool fHasDefaultReadFunction() const;
  virtual decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const {
    return lReadoutInterval.fGetValue();
  }
  virtual void fReadCurrentValue() {};
  virtual void fSendValues() = 0;
  int32_t fGetUid() const {return lUid;};
virtual void fConfigure();
};


template <typename T> class SlowcontrolMeasurement: public SlowcontrolMeasurementBase {
 public:
  class timedValue {
  public:
    std::chrono::system_clock::time_point lTime;
    T lValue;
    timedValue() {};
  timedValue(decltype(lTime) aTime, decltype(lValue) aValue) :
    lTime(aTime), lValue(aValue) {};
  };
 protected:
  std::vector<timedValue> lValues;
  std::deque<timedValue> lSendQueue;
  configValue<T> lDeadBand;

  virtual void fSendValue(const timedValue& aValue) = 0;
  
  
 public:
 SlowcontrolMeasurement() : lDeadBand("DeadBand",lConfigValues) {};
  T fAbs(T aValue) {
    if (aValue > 0) {
      return aValue;
    } else {
      return -aValue;
    }
  };
  virtual void fStore(const T& aValue) {
    fStore(aValue, std::chrono::system_clock::now());
  };
  virtual void fStore(const T& aValue, std::chrono::system_clock::time_point aTime) {
    if ((lValues.size() > 2)
	&& lValues.at(lValues.size()-1).lValue == aValue
	&& lValues.at(lValues.size()-2).lValue == aValue) { // no change
      lValues.at(lValues.size()-1).lTime = aTime;
    } else {
      lValues.emplace_back(aTime,aValue);
    }
    if (aValue < lValues.at(lMinValueIndex).lValue) {
      lMinValueIndex = lValues.size()-1;
    }
    if (aValue > lValues.at(lMaxValueIndex).lValue) {
      lMaxValueIndex = lValues.size()-1;
    }
    if (fAbs(aValue - lValues.front().lValue) > lDeadBand ||
	lValues.back().lTime - lValues.front().lTime > lMaxDeltaT.fGetValue() ||
	lValues.size()==1) {
      std::set<size_t> indicesToSend;
      if (lMinValueIndex > 0) {
	indicesToSend.insert(lMinValueIndex);
      }
      if (lMaxValueIndex > 0) {
	indicesToSend.insert(lMaxValueIndex);
      }
      indicesToSend.insert(lValues.size()-1);
      { // scope for send queue locking
	std::lock_guard<std::mutex> SendQueueLock(lSendQueueMutex);
	for (auto index : indicesToSend) {
	  lSendQueue.push_back(lValues.at(index));
	}
      }
      slowcontrolDaemon::fGetInstance()->fSignalToStorer();
      auto lastValue = lValues.back();
      lValues.clear();
      lValues.push_back(lastValue);
      lMinValueIndex = 0;
      lMaxValueIndex = 0;
    }
  };
  
  virtual void fSendValues() {
    while (! lSendQueue.empty()) { // empty() is thread safe by itself 
      timedValue value;
      { // scope for send queue locking
	std::lock_guard<std::mutex> SendQueueLock(lSendQueueMutex);
	value = lSendQueue.front();
	lSendQueue.pop_front();
      }
      fSendValue(value);
    };
  };
};

class SlowcontrolMeasurementFloat: public SlowcontrolMeasurement<float> {
 protected:
  const char *fGetDefaultTableName() const {return "measurements_float";};
 public:
  virtual void fSendValue(const timedValue& aValue);
};

#endif

