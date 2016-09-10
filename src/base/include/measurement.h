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

namespace slowcontrol {

	class measurementBase {
	  public:
		typedef int32_t uidType;
		typedef std::chrono::system_clock::time_point timeType;
		typedef std::chrono::system_clock::duration durationType;
	  protected:
		configValueBase::mapType lConfigValues;
	  public:
	  protected:
		std::mutex lSendQueueMutex;
		uidType lUid;
		measurement_state::stateType lState;
		configValue<std::string> lClassName;
		virtual const char *fGetDefaultTableName() const = 0;
		void fSaveOption(const configValueBase& aCfgValue,
		                 const char *comment);
		virtual void fInitializeUid(const std::string& aDescription);
	  public:
		measurementBase();
		virtual void fFlush(bool aFlushSingleValue = false) = 0;
		virtual void fSendValues() = 0;
		virtual bool fValuesToSend() = 0;
		uidType fGetUid() const {
			return lUid;
		};
		virtual void fConfigure();
		virtual measurement_state::stateType fSetState(const std::string& aStateName,
		        const std::string& aReason);
	};

	class defaultReaderInterface {
		configValue<measurementBase::durationType> lReadoutInterval;
	  public:
		defaultReaderInterface(configValueBase::mapType& aMap,
		                       decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval): lReadoutInterval("readoutInterval", aMap, aDefaultReadoutInterval) {};
		virtual decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const {
			return lReadoutInterval.fGetValue();
		}
		virtual void fReadCurrentValue() = 0;
	};


	class writeValue {
	  public:
		class request {
		  public:
			typedef std::chrono::system_clock::time_point timeType;
			typedef int idType;
		  protected:
			timeType lWhen;
			idType lRequestId;
			writeValue* lWriteValue;
		  public:
			timeType fGetWhen() const {
				return lWhen;
			};
			idType fGetRequestId() const {
				return lRequestId;
			};
			writeValue* fGetWriteValue() const {
				return lWriteValue;
			};
			request(writeValue* aWriteValue,
			        timeType aWhen,
			        idType aRequestId): lWhen(aWhen),
				lRequestId(aRequestId),
				lWriteValue(aWriteValue) {};
			virtual ~request() {};
			virtual bool fProcess(std::string& aResponse) {
				return fGetWriteValue()->fProcessRequest(this, aResponse);
			};
		};
		virtual request* fParseForRequest(const std::string& aRequestText,
		                                  request::timeType aWhen,
		                                  request::idType aRequestId) = 0;
		virtual bool fProcessRequest(const request* aRequest, std::string& aResponse) = 0;
	};

	template <typename T> class writeValueWithType: public writeValue {
	  public:
		class requestWithType: public request {
		  public:
			T lGoalValue;
			requestWithType(writeValue* aWriteValue,
			                timeType aWhen,
			                idType aRequestId,
			                decltype(lGoalValue) aGoalValue):
				request(aWriteValue, aWhen, aRequestId), lGoalValue(aGoalValue) {};
			virtual ~requestWithType() {};
		};
		virtual request* fParseForRequest(const std::string& aRequestText,
		                                  request::timeType aWhen,
		                                  request::idType aRequestId) {
			T value;
			if (fParseForSet(aRequestText, value)) {
				return new requestWithType(this, aWhen, aRequestId, value);
			}
			return nullptr;
		};
		virtual bool fParseForSet(const std::string& aRequestText, T& aValue) {
			std::istringstream buf(aRequestText);
			std::string command;
			buf >> command;
			if (command.compare("set") == 0) {
				buf >> aValue;
				return !buf.fail();
			}
			return false;
		};
	};
	typedef writeValue::request requestType;
	class unitInterface {
	  protected:
		configValue<std::string> lUnit;
	  public:
		unitInterface(configValueBase::mapType& aMap, const char *aUnit):
			lUnit("unit", aMap, aUnit) {
		}
	};


	template <typename T> class measurement: public measurementBase {
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
			measurementBase::fConfigure();
			if (!lValues.empty()) {
				fCheckValue(lValues.back().lTime, lValues.back().lValue);
			}
		};
	  public:
		measurement(decltype(lDeadBand.fGetValue()) aDefaultDeadBand) :
			measurementBase(),
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

		virtual void fFlush(bool aFlushSingleValue) {
			if (!lValues.empty() && (aFlushSingleValue || lValues.size() > 1)) {
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
				daemon::fGetInstance()->fSignalToStorer();
				auto lastValue = lValues.back();
				lValues.clear();
				lValues.push_back(lastValue);
				lMinValueIndex = 0;
				lMaxValueIndex = 0;
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
			if (fAbs(aValue - lValues.front().lValue) >= lDeadBand ||
			        lValues.size() == 1) {
				fFlush(lValues.size() == 1);
			}
		};

		virtual bool fValuesToSend() {
			return ! lSendQueue.empty();
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
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			PQclear(result);
		};
	};

	template <> class measurement<bool>: public measurementBase {
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
		measurement():
			measurementBase() {
			lNoValueYet = true;
			lOldValueUnsent = true;
		};

		virtual void fFlush(bool /*aFlushSingleValue*/) {
			if (lOldValueUnsent && ! lNoValueYet) {
				std::lock_guard<decltype(lSendQueueMutex)> SendQueueLock(lSendQueueMutex);
				lSendQueue.emplace_back(lOldTime, lOldValue);
				lOldValueUnsent = false;
			}
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
				daemon::fGetInstance()->fSignalToStorer();
			} else {
				lOldTime = aTime;
				lOldValueUnsent = true;
			}
		};

		virtual bool fValuesToSend() {
			return ! lSendQueue.empty();
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
			auto result = PQexec(base::fGetDbconn(), query.c_str());
			PQclear(result);
		};
	};

	template <typename T> class dummyConfigValue {
	  public:
		dummyConfigValue(const char */*aName*/,
		                 configValueBase::mapType& /*aMap*/,
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
		boundCheckerInterface(decltype(baseClass::lDeadBand.fGetValue()) aDefaultDeadBand,
		                      decltype(lLowerBound.fGetValue()) aLowerBound,
		                      decltype(lUpperBound.fGetValue()) aUpperBound) :
			baseClass(aDefaultDeadBand),
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


} // end of namespace slowcontrol
#endif

