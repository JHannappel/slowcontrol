#ifndef __gpio_h_
#define __gpio_h_
#include <measurement.h>
#include <string>

namespace slowcontrol {
	namespace gpio {
		class pin_base {
		  protected:
			unsigned int lPinNumber;
			std::string lDirPath;
			int lValueFd;
		  public:
			pin_base(unsigned int aPinNumber);
			bool fRead();
			int fGetFd() {
				return lValueFd;
			};
		};

		class input: public pin_base {
		  public:
			input(unsigned int aPinNumber);
		};
		class output: public pin_base {
		  public:
			output(unsigned int aPinNumber);
			void fWrite(bool aValue);
		};


		class input_value: public measurement<bool>,
			public pollReaderInterface {
		  protected:
			input lInPin;
			configValue<measurementBase::durationType> lReadDelay;
			configValue<bool> lInvert;
		  public:
			input_value(const std::string &aName,
			            unsigned int aPinNumber);
			virtual int fGetFd() {
				return lInPin.fGetFd();
			}
			virtual void fSetPollFd(struct pollfd *aPollfd);
			virtual bool fProcessData(short /*aRevents*/);

		};
		class output_value: public measurement<bool>,
			public writeValueWithType<bool> {
		  protected:
			output lOutPin;
		  public:
			output_value(const std::string &aName,
			             unsigned int aPinNumber);

			virtual bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse);
			virtual void fSet(bool aValue);
		};

		class timediff_value: public boundCheckerInterface<measurement<float>>,
			        public defaultReaderInterface {
		  protected:
			input lInPin;
			output lOutPin;
		  public:
			timediff_value(const std::string& aName, unsigned int aInPin, unsigned int aOutPin);
			virtual bool fReadCurrentValue();
		};
	} // end of namespace gpio
} // end of namespace slowcontrol

#endif
