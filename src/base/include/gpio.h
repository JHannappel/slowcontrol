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
			int fGetFd() {
				return lValueFd;
			};
		};

		class input: public pin_base {
		  public:
			input(unsigned int aPinNumber);
			bool fRead();
		};
		class output: public pin_base {
		  public:
			output(unsigned int aPinNumber);
			void fWrite(bool aValue);
		};


		class input_value: public measurement<bool>,
			public pollReaderInterface,
			public input {
		  public:
			input_value(const std::string &aName,
			            unsigned int aPinNumber);
			virtual int fGetFd() {
				return pin_base::fGetFd();
			}
			virtual void fSetPollFd(struct pollfd *aPollfd);
			virtual void fProcessData(short /*aRevents*/);

		};
		class output_value: public measurement<bool>,
			public writeValueWithType<bool>,
			public output {
		  public:
			output_value(const std::string &aName,
			             unsigned int aPinNumber);

			virtual bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse);
		};

		class timediff_value: public boundCheckerInterface<measurement<float>>,
			        public defaultReaderInterface {
		  protected:
			input lInPin;
			output lOutPin;
		  public:
			timediff_value(const std::string& aName, unsigned int aInPin, unsigned int aOutPin);
			virtual void fReadCurrentValue();
		};
	} // end of namespace gpio
} // end of namespace slowcontrol

#endif
