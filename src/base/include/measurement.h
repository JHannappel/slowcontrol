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
  public:
	configValue<std::chrono::system_clock::duration> lMaxDeltaT;
	configValue<std::chrono::system_clock::duration> lReadoutInterval;
  protected:
	std::mutex lSendQueueMutex;
	size_t lMinValueIndex;
	size_t lMaxValueIndex;
	int32_t lUid;
	virtual const char *fGetDefaultTableName() const = 0;
	void fSaveOption(const configValueBase& aCfgValue,
	                 const char *comment);
	virtual void fInitializeUid(const std::string& aDescription);
  public:
	SlowcontrolMeasurementBase(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                           decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval);
	virtual decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const {
		return lReadoutInterval.fGetValue();
	}
	virtual void fSendValues() = 0;
	int32_t fGetUid() const {
		return lUid;
	};
	virtual void fConfigure();
};

class defaultReaderInterface {
  public:
	virtual void fReadCurrentValue() = 0;
};

template <typename T> class SlowcontrolMeasurement: public SlowcontrolMeasurementBase {
  public:
	typedef T valueType;
	typedef std::chrono::system_clock::time_point timeType;
	class timedValue {
	  public:
		timeType lTime;
		T lValue;
		timedValue() {};
		timedValue(decltype(lTime) aTime, decltype(lValue) aValue) :
			lTime(aTime), lValue(aValue) {};
	};
  protected:
	std::vector<timedValue> lValues;
	std::deque<timedValue> lSendQueue;
  public:
	configValue<T> lDeadBand;
  protected:
	virtual void fSendValue(const timedValue& aValue) = 0;
	virtual void fCheckValue(timeType /*aTime*/,
	                         T /*aValue*/) {};

  public:
	SlowcontrolMeasurement(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                       decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval,
	                       decltype(lDeadBand.fGetValue()) aDefaultDeadBand) :
		SlowcontrolMeasurementBase(aDefaultMaxDeltat, aDefaultReadoutInterval),
		lDeadBand("DeadBand", lConfigValues, aDefaultDeadBand) {};
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
	virtual void fStore(const T& aValue, timeType aTime) {
		if ((lValues.size() > 2)
		        && lValues.at(lValues.size() - 1).lValue == aValue
		        && lValues.at(lValues.size() - 2).lValue == aValue) { // no change
			lValues.at(lValues.size() - 1).lTime = aTime;
		} else {
			fCheckValue(aTime, aValue);
			lValues.emplace_back(aTime, aValue);
		}
		if (aValue < lValues.at(lMinValueIndex).lValue) {
			lMinValueIndex = lValues.size() - 1;
		}
		if (aValue > lValues.at(lMaxValueIndex).lValue) {
			lMaxValueIndex = lValues.size() - 1;
		}
		if (fAbs(aValue - lValues.front().lValue) > lDeadBand ||
		        lValues.back().lTime - lValues.front().lTime > lMaxDeltaT.fGetValue() ||
		        lValues.size() == 1) {
			std::set<size_t> indicesToSend;
			if (lMinValueIndex > 0) {
				indicesToSend.insert(lMinValueIndex);
			}
			if (lMaxValueIndex > 0) {
				indicesToSend.insert(lMaxValueIndex);
			}
			indicesToSend.insert(lValues.size() - 1);
			{
				// scope for send queue locking
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
			{
				// scope for send queue locking
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
	const char *fGetDefaultTableName() const {
		return "measurements_float";
	};
  public:
	SlowcontrolMeasurementFloat(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                            decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval,
	                            decltype(lDeadBand.fGetValue()) aDefaultDeadBand);
	virtual void fSendValue(const timedValue& aValue);
};


template <typename baseClass> class boundCheckerInterface: public baseClass {
  protected:
	configValue<typename baseClass::valueType> lLowerBound;
	configValue<typename baseClass::valueType> lUppperBound;
  public:
	boundCheckerInterface(decltype(baseClass::lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                      decltype(baseClass::lReadoutInterval.fGetValue()) aDefaultReadoutInterval,
	                      decltype(baseClass::lDeadBand.fGetValue()) aDefaultDeadBand,
	                      decltype(lLowerBound.fGetValue()) aLowerBound,
	                      decltype(lUppperBound.fGetValue()) aUpperBound) :
		baseClass(aDefaultMaxDeltat, aDefaultReadoutInterval, aDefaultDeadBand),
		lLowerBound("lowerBound", this->lConfigValues, aLowerBound),
		lUppperBound("upperBound", this->lConfigValues, aUpperBound) {
	};
	virtual void fCheckValue(typename baseClass::timeType /*aTime*/,
	                         typename baseClass::valueType aValue) {
		if (lLowerBound.fGetValue() > aValue ||
		        lUppperBound.fGetValue() < aValue) {
			std::cout << " value out of bounds " << aValue;
		}
	};
};

#endif

