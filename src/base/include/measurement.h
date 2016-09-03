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
#include "states.h"

class SlowcontrolMeasurementBase {
  public:
	typedef int32_t uidType;
	typedef std::chrono::system_clock::time_point timeType;
  protected:
	std::map<std::string, configValueBase*> lConfigValues;
  public:
	configValue<std::chrono::system_clock::duration> lMaxDeltaT;
	configValue<std::chrono::system_clock::duration> lReadoutInterval;
  protected:
	std::mutex lSendQueueMutex;
	uidType lUid;
	measurement_state::stateType lState;
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
	uidType fGetUid() const {
		return lUid;
	};
	virtual void fConfigure();
	virtual measurement_state::stateType fSetState(const std::string& aStateName,
	        const std::string& aReason);
};

class defaultReaderInterface {
  public:
	virtual void fReadCurrentValue() = 0;
};


class writeValueInterface {
  public:
	virtual const std::string fProcessRequest(const std::string& aRequest) = 0;
};

template <typename T> class SlowcontrolMeasurement: public SlowcontrolMeasurementBase {
  public:
	typedef T valueType;
	class timedValue {
	  public:
		timeType lTime;
		T lValue;
		timedValue() {};
		timedValue(decltype(lTime) aTime, decltype(lValue) aValue) :
			lTime(aTime), lValue(aValue) {};
	};
  protected:
	virtual const char *fGetDefaultTableName() const {
		if (std::is_integral<T>::value && sizeof(T) <= 2) {
			return "measurements_int2";
		} else if (std::is_integral<T>::value && sizeof(T) <= 4) {
			return "measurements_int4";
		} else if (std::is_integral<T>::value && sizeof(T) <= 8) {
			return "measurements_int8";
		} else if (std::is_floating_point<T>::value && sizeof(T) == 4) {
			return "measurements_float";
		}
	};

	std::vector<timedValue> lValues;
	size_t lMinValueIndex;
	size_t lMaxValueIndex;
	std::deque<timedValue> lSendQueue;
  public:
	configValue<T> lDeadBand;
  protected:
	virtual void fCheckValue(timeType /*aTime*/,
	                         T /*aValue*/) {};
	virtual void fConfigure() {
		SlowcontrolMeasurementBase::fConfigure();
		if (!lValues.empty()) {
			fCheckValue(lValues.back().lTime, lValues.back().lValue);
		}
	};
  public:
	SlowcontrolMeasurement(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                       decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval,
	                       decltype(lDeadBand.fGetValue()) aDefaultDeadBand) :
		SlowcontrolMeasurementBase(aDefaultMaxDeltat, aDefaultReadoutInterval),
		lDeadBand("DeadBand", lConfigValues, aDefaultDeadBand) {
		lMinValueIndex = 0;
		lMaxValueIndex = 0;
	};
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
				std::lock_guard<decltype(lSendQueueMutex)> SendQueueLock(lSendQueueMutex);
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
	virtual void fSendValue(const timedValue& aValue) {
		std::string query("INSERT INTO ");
		query += fGetDefaultTableName();
		query += " (uid, time, value) VALUES ( ";
		query += std::to_string(fGetUid());
		query += ", (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
		query += std::to_string(std::chrono::duration<double, std::nano>(aValue.lTime.time_since_epoch()).count() / 1E9);
		query += " * INTERVAL '1 second'),";
		query += std::to_string(aValue.lValue);
		query += " );";
		auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
		PQclear(result);
	};
};

template <> class SlowcontrolMeasurement<bool>: public SlowcontrolMeasurementBase {
  public:
	typedef bool valueType;
	class timedValue {
	  public:
		timeType lTime;
		bool lValue;
		timedValue() {};
		timedValue(decltype(lTime) aTime, decltype(lValue) aValue) :
			lTime(aTime), lValue(aValue) {};
	};
  protected:
	virtual const char *fGetDefaultTableName() const {
		return "measurements_bool";
	};
	bool lOldValue;
	bool lNoValueYet;
	bool lOldValueUnsent;
	timeType lOldTime;
	std::deque<timedValue> lSendQueue;
  public:
	SlowcontrolMeasurement(decltype(lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                       decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval):
		SlowcontrolMeasurementBase(aDefaultMaxDeltat, aDefaultReadoutInterval) {
		lNoValueYet = true;
		lOldValueUnsent = true;
	};
	virtual void fStore(bool aValue) {
		fStore(aValue, std::chrono::system_clock::now());
	};
	virtual void fStore(bool aValue, timeType aTime) {
		if (lNoValueYet || aValue != lOldValue) {
			{
				std::lock_guard<decltype(lSendQueueMutex)> SendQueueLock(lSendQueueMutex);
				if (lOldValueUnsent) {
					lSendQueue.emplace_back(lOldTime, lOldValue);
				}
				lSendQueue.emplace_back(aTime, aValue);
			}
			lNoValueYet = false;
			lOldValue = aValue;
			lOldValueUnsent = false;
			slowcontrolDaemon::fGetInstance()->fSignalToStorer();
		} else {
			lOldTime = aTime;
			lOldValueUnsent = true;
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
	virtual void fSendValue(const timedValue& aValue) {
		std::string query("INSERT INTO ");
		query += fGetDefaultTableName();
		query += " (uid, time, value) VALUES ( ";
		query += std::to_string(fGetUid());
		query += ", (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
		query += std::to_string(std::chrono::duration<double, std::nano>(aValue.lTime.time_since_epoch()).count() / 1E9);
		query += " * INTERVAL '1 second'),";
		query += aValue.lValue ? "'t'" : "'f'";
		query += " );";
		auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
		PQclear(result);
	};
};

template <typename T> class dummyConfigValue {
  public:
	dummyConfigValue(const char */*aName*/,
	                 std::map<std::string, configValueBase*>& /*aMap*/,
	                 const T& /*aValue*/ = 0) {};
	T fGetValue() const {
		return 0;
	};
};

template <typename baseClass, bool checkLowBound = true, bool checkHighBound = true> class boundCheckerInterface: public baseClass {
  protected:
	typename std::conditional<checkLowBound, configValue<typename baseClass::valueType>,
	         dummyConfigValue<typename baseClass::valueType>>::type lLowerBound;
	typename std::conditional<checkHighBound, configValue<typename baseClass::valueType>,
	         dummyConfigValue<typename baseClass::valueType>>::type lUpperBound;
	measurement_state::stateType lNormalType = 0;
	measurement_state::stateType lLowValueType = 0;
	measurement_state::stateType lHighValueType = 0;
  public:
	boundCheckerInterface(decltype(baseClass::lMaxDeltaT.fGetValue()) aDefaultMaxDeltat,
	                      decltype(baseClass::lReadoutInterval.fGetValue()) aDefaultReadoutInterval,
	                      decltype(baseClass::lDeadBand.fGetValue()) aDefaultDeadBand,
	                      decltype(lLowerBound.fGetValue()) aLowerBound,
	                      decltype(lUpperBound.fGetValue()) aUpperBound) :
		baseClass(aDefaultMaxDeltat, aDefaultReadoutInterval, aDefaultDeadBand),
		lLowerBound("lowerBound", this->lConfigValues, aLowerBound),
		lUpperBound("upperBound", this->lConfigValues, aUpperBound) {
	};
	virtual void fCheckValue(typename baseClass::timeType /*aTime*/,
	                         typename baseClass::valueType aValue) {
		if (checkLowBound && lLowerBound.fGetValue() > aValue) {
			if (this->lState != lLowValueType) {
				std::string reason("value is ");
				reason += std::to_string(aValue);
				reason += ", limit is ";
				reason += std::to_string(lLowerBound.fGetValue());
				auto lowValueType = this->fSetState("valueTooLow", reason);
				if (lLowValueType == 0) {
					lLowValueType = lowValueType;
				}
			}
		} else if (checkHighBound && lUpperBound.fGetValue() < aValue) {
			if (this->lState != lHighValueType) {
				std::string reason("value is ");
				reason += std::to_string(aValue);
				reason += ", limit is ";
				reason += std::to_string(lUpperBound.fGetValue());
				auto highValueType = this->fSetState("valueTooHigh", reason);
				if (lHighValueType == 0) {
					lHighValueType = highValueType;
				}
			}
		} else { // value inside normal bounds
			if (this->lState != lNormalType) {
				std::string reason("value is ");
				reason += std::to_string(aValue);
				reason += ", limit is ";
				if (this->lState == lLowValueType) {
					reason += std::to_string(lLowerBound.fGetValue());
				} else {
					reason += std::to_string(lUpperBound.fGetValue());
				}
				auto normalType = this->fSetState("normal", reason);
				if (lNormalType == 0) {
					lNormalType = normalType;
				}
			}
		}
	};
};

#endif

