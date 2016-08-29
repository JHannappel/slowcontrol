#ifndef __states_h_
#define __states_h_
#include <mutex>
#include <map>

class measurement_state {
  protected:
	static std::mutex gStatesMutex;
	static std::map<std::string, int32_t> gStates;
  public:
	static int32_t fGetState(const std::string& aStateName);
};


#endif
