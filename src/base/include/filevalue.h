#ifndef __filevalue_h_
#define __filevalue_h_
#include <measurement.h>
#include <string>
#include <iostream>

namespace slowcontrol {
	namespace filevalue {
		template <typename T, class scale = std::ratio<1>, typename ReadType = T> class input:
			public measurement<T> {
		  protected:
			std::string lPath;
		  public:
			template <class ... Types> input(
			    const std::string &aPath,
			    Types ... args
			) :
				measurement<T>(args ...),
				lPath(aPath) {
			}
			virtual bool fReadCurrentValue() {
				ReadType value;
				std::ifstream file(lPath);
				file >> value;
				T storeValue = (value * scale::num) / scale::den ;
				return this->fStore(storeValue);
			}
		};
		/*
		template <typename T, class scale=std::ration<1>, bool readBack=true, typename WriteType=T> class output:
			public measurement<T>,
			public writeValueWithType<T> {
		protected:
			std::string lPath;
		public:
			output(const std::string &aPath) : lPath(aPath) {
			}

			virtual bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) {
				auto req = dynamic_cast<const requestWithType*>(aRequest);
				WriteType scaledValue = (req->lGoalValue * scale::den) / scale::num;
				if (req != nullptr) {
					{
						std::ofstream file(lPath);
						file << scaledValue;
					}
					if (readBack) {
						std::ifstream file(lPath);
						WriteType value;
						file >> value;
						T storeValue=(value * scale::num) / scale::den;
						fStore(storeValue);
						if (value == scaledValue) {
							aResponse = "done.";
						} else {
							aResponse = "failed, got ";
							aResponse += std::to_string(value);
							aResponse += " instead of ";
							aResponse += std::to_string(scaledValue);
						}
					} else {
						aResponse = "done.";
				}
				return true;
				}
				aResponse = "can't cast request";
				return false;
			}
		};
		*/
	} // end of namespace filevalue
} // end of namespace slowcontrol

#endif
