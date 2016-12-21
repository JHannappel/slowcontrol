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
#include <poll.h>

#include "configValue.h"
#include "slowcontrolDaemon.h"
#include "slowcontrol.h"
#include "states.h"

namespace slowcontrol {

	/// base class for measurements

	/// it provides an interface for the basics tasks that all measurements must know.
	/// Most of the fuctions are pure virtual and must be defined in the classes inheriting
	/// from this one.

	class measurementBase {
	  public:
		typedef std::chrono::system_clock::time_point timeType; ///< type for the time points at wich measurements occur
		typedef std::chrono::system_clock::duration durationType; ///< type for durations matching the timeType
	  protected:
		configValueBase::mapType lConfigValues; ///< map of config values, needed also as parameter for the config value constructors
	  protected:
		std::mutex lSendQueueMutex; ///< mutex for protecting the value send queue, used in measurement
		base::uidType lUid;               ///< the uid of this measurement
		measurement_state::stateType lState;  ///< the state of this measurement, \sa measurement_state
		configValue<std::string> lClassName;  ///< shall hold the class name, must be set in the most derived contstructor, for exsmples \sa owTemperature diskValue

		virtual const char *fGetDefaultTableName() const = 0;  ///< returns the name of the table in the database
		void fSaveOption(const configValueBase& aCfgValue,
		                 const char *comment); ///< saves an option's current value in the database
		virtual void fInitializeUid(const std::string& aDescription); ///< sets and registers uid in database, does post-constructor initialisation
	  public:
		measurementBase();
		virtual void fFlush(bool aFlushSingleValue = false) = 0; ///< write current value into database
		virtual void fSendValues() = 0; ///< send values in send queue to database
		virtual bool fValuesToSend() = 0; ///< check if there are values to send
		decltype(lUid) fGetUid() const {  ///< get the UID
			return lUid;
		};
		virtual void fConfigure(); ///< (re-)read the config values from the database
		virtual measurement_state::stateType fSetState(const std::string& aStateName,
		        const std::string& aReason); ///< set state by name
	};

	class defaultReaderInterface { ///< interface for measurements that can read themselves.
	  protected:
		configValue<measurementBase::durationType> lReadoutInterval;
	  public:
		defaultReaderInterface(configValueBase::mapType& aMap,
		                       decltype(lReadoutInterval.fGetValue()) aDefaultReadoutInterval): lReadoutInterval("readoutInterval", aMap, aDefaultReadoutInterval) {};
		virtual decltype(lReadoutInterval.fGetValue()) fGetReadoutInterval() const {
			return lReadoutInterval.fGetValue();
		}
		virtual void fReadCurrentValue() = 0;
	};

	class pollReaderInterface { ///< interface for measurements that depend on a poll() able fd
	  public:
		virtual int fGetFd() = 0;
		virtual void fSetPollFd(struct pollfd *aPollfd) = 0;
		virtual void fProcessData(short aRevents) = 0;
	};


	class writeValue { ///< interface for measurements that can be set, i.e. write values
	  public:
		class request { ///< class to represent one write request, needs specialisation to do something
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

	template <typename T> class writeValueWithType: public writeValue { ///< templated specialisation for real use
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
			// optimisation should squash this to s single return for each template instance
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
		std::atomic<T> lCurrentValue;
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
		T fGetCurrentValue() {
			return lCurrentValue.load();
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
			lCurrentValue = aValue;
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
			if (aValue >= lValues.at(lMinValueIndex).lValue + lDeadBand ||
			        aValue <= lValues.at(lMaxValueIndex).lValue - lDeadBand ||
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
		std::atomic<bool> lCurrentValue;
	  public:
		measurement():
			measurementBase() {
			lNoValueYet = true;
			lOldValueUnsent = true;
		};
		bool fGetCurrentValue() {
			return lCurrentValue.load();
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
			lCurrentValue = aValue;
			if (lNoValueYet || aValue != lOldValue) {
				{
					std::lock_guard<decltype(lSendQueueMutex)> SendQueueLock(lSendQueueMutex);
					if (lOldValueUnsent) {
						lSendQueue.emplace_back(lOldTime, lOldValue);
						lOldTime = aTime - std::chrono::microseconds(1);
						lSendQueue.emplace_back(lOldTime, lOldValue);
					}
					lSendQueue.emplace_back(aTime, aValue);
				}
				lNoValueYet = false;
				lOldValue = aValue;
				lOldValueUnsent = false;
				daemon::fGetInstance()->fSignalToStorer();
			} else {
				lOldValueUnsent = true;
			}
			lOldTime = aTime;
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
		template <class ... Types> boundCheckerInterface(
		    decltype(lLowerBound.fGetValue()) aLowerBound,
		    decltype(lUpperBound.fGetValue()) aUpperBound,
		    Types ... args
		) :
			baseClass(args ...),
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


	template <typename T> class watched_base {
	  protected:
		bool (*lWatchCondition)(T *aThat) ;
		std::condition_variable lWaitCondition;
		std::mutex lWaitMutex;
	  public:
		virtual bool fWaitForChange() {
			std::unique_lock < decltype(lWaitMutex) > lock(lWaitMutex);
			auto waitResult = lWaitCondition.wait_for(lock, std::chrono::seconds(1));
			return waitResult == std::cv_status::no_timeout;
		}
	};
	template <typename T> class watched_poller: public T, public watched_base<T> {
	  public:
		template<class ... Types> watched_poller (Types ... args) :
			T(args...) {
		};
		virtual void fProcessData(short aRevents) override {
			std::cout << __FILE__ << __LINE__ << "\n";
			T::fProcessData(aRevents);
			if (this->lWatchCondition(this)) {
				this->lWaitCondition.notify_all();
			}
		};
	};
	template <typename T> class watched_reader: public T, public watched_base<T> {
	  public:
		template<class ... Types> watched_reader (Types ... args) :
			T(args...) {
		};
		virtual void fReadCurrentValue() override {
			std::cout << __FILE__ << __LINE__ << "\n";
			T::fReadCurrentValue();
			if (this->lWatchCondition(this)) {
				this->lWaitCondition.notify_all();
			}
		};
	};

	template <typename T> class watched_measurement: public std::conditional<std::is_base_of<pollReaderInterface, T>::value,
		watched_poller<T>,
		typename std::conditional<std::is_base_of<defaultReaderInterface, T>::value,
		watched_reader<T>,
		T>::type>::type {
	  public:
		template<class ... Types> watched_measurement (bool (*aWatchCondition)(T* aThat),
		        Types ... args) :
			std::conditional<std::is_base_of<pollReaderInterface, T>::value,
			watched_poller<T>,
			typename std::conditional<std::is_base_of<defaultReaderInterface, T>::value,
			watched_reader<T>,
			T>::type>::type (args...) {
			this->lWatchCondition = aWatchCondition;
		};
	};



} // end of namespace slowcontrol
#endif

