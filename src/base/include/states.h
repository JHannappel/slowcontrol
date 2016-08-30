#ifndef __states_h_
#define __states_h_
#include <mutex>
#include <map>

class measurement_state {
  public:
	typedef int32_t stateType;
  protected:
	static std::mutex gStatesMutex;
	static std::map<std::string, stateType> gStates;
  public:
	static stateType fGetState(const std::string& aStateName);
};


#endif
