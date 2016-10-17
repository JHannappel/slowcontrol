#include "measurement.h"

namespace slowcontrol {
	class parasitic_temperature: public boundCheckerInterface<measurement<float>>,
		        public unitInterface {
	  public:
		parasitic_temperature(const char *aName,
		                      float aDeadBand = 0.5,
		                      float aMinAllowed = -40,
		                      float aMaxAllowed = 100):
			boundCheckerInterface(aDeadBand, aMinAllowed, aMaxAllowed),
			unitInterface(lConfigValues, "deg C") {
			lClassName.fSetFromString(__func__);
			fInitializeUid(aName);
			fConfigure();
		};
	};
}
