#ifndef __configValue_h_
#define __configValue_h_
#include <sstream>
#include <atomic>
#include <mutex>
#include <thread>
class configValueBase {
	const std::string lName;
  public:
	typedef std::map<std::string, configValueBase*> mapType;
	configValueBase(const char* aName,
	                mapType& aMap):
		lName(aName) {
		aMap.emplace(aName, this);
	};
	virtual void fSetFromString(const char *aString) = 0;
	virtual void fAsString(std::string& aString) const = 0;
	const std::string& fGetName() const {
		return lName;
	};
};

template <typename T> class configValue: public configValueBase {
	std::atomic<T> lValue;
  public:
	configValue(const char *aName,
	            mapType& aMap):
		configValueBase(aName, aMap) {};
	configValue(const char *aName,
	            mapType& aMap,
	            const T& aValue):
		configValueBase(aName, aMap), lValue(aValue) {};
	T fGetValue() const {
		return lValue.load();
	};
	operator T () const {
		return lValue.load();
	}
	virtual void fSetValue(T aValue) {
		lValue = aValue;
	}
	virtual void fSetFromString(const char *aString) {
		std::stringstream buf(aString);
		T buf2;
		buf >> buf2;
		lValue = buf2;
	}
	virtual void fAsString(std::string& aString) const {
		std::ostringstream buf("");
		buf << fGetValue();
		aString += buf.str();
	}
};

template <> class configValue<std::chrono::system_clock::duration>: public configValueBase {
	std::atomic<std::chrono::system_clock::duration> lValue;
  public:
	configValue(const char *aName,
	            mapType& aMap):
		configValueBase(aName, aMap) {
	};
	configValue(const char *aName,
	            mapType& aMap,
	            const std::chrono::system_clock::duration& aValue):
		configValueBase(aName, aMap), lValue(aValue) {
	};
	std::chrono::system_clock::duration fGetValue() const {
		return lValue.load();
	};
	operator std::chrono::system_clock::duration () const {
		return lValue;
	}
	virtual void fSetValue(std::chrono::system_clock::duration aValue) {
		lValue = aValue;
	}
	virtual void fSetFromString(const char *aString) {
		double d = std::stod(aString);
		std::chrono::microseconds tmp(static_cast<long long>(d * 1E6));
		lValue = tmp;
	}
	virtual void fAsString(std::string& aString) const {
		auto a = std::to_string(std::chrono::duration_cast<std::chrono::duration<double>>(lValue.load()).count());
		aString += a;
	}
};

template <> class configValue<std::string>: public configValueBase {
  protected:
	std::string lValue;
	mutable std::mutex lMutex;
  public:
	configValue(const char *aName,
	            mapType& aMap):
		configValueBase(aName, aMap) {
	};
	configValue(const char *aName,
	            mapType& aMap,
	            const std::string& aValue):
		configValueBase(aName, aMap), lValue(aValue) {
	};
	std::string	fGetValue() const {
		std::lock_guard<decltype(lMutex)> lock(lMutex);
		auto copy = lValue;
		return copy;
	}
	operator std::string	() const {
		std::lock_guard<decltype(lMutex)> lock(lMutex);
		auto copy = lValue;
		return copy;
	}
	void fSetValue(std::string aValue)  {
		std::lock_guard<decltype(lMutex)> lock(lMutex);
		lValue = aValue;
	}
	virtual void fSetFromString(const char *aString) {
		std::lock_guard<decltype(lMutex)> lock(lMutex);
		lValue = aString;
	}
	virtual void fAsString(std::string& aString) const {
		std::lock_guard<decltype(lMutex)> lock(lMutex);
		aString = lValue;
	}
};

#endif
