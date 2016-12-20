#ifndef __communications_h_
#define __communications_h_
#include <chrono>
#include <string>

namespace slowcontrol {
	class communicationChannel {
	  public:
		typedef std::chrono::system_clock::time_point timeType; ///<type for time measurements
		typedef std::chrono::system_clock::duration durationType; ///< type for durations matching the timeType
	  protected:
		int lFd;
		timeType lLastCommunicationTime;
		timeType lReadStartTime;
	  public:
		communicationChannel() {};
		virtual ~communicationChannel() {};
		int fGetFd() const {
			return lFd;
		};
		virtual bool fInit() = 0;
		virtual bool fStatus() {
			return true;
		};
		virtual bool fWrite(const char *aData);
		virtual int fRead(char *aBuffer, int aBuffsize, durationType aTimeout = std::chrono::seconds(1)) = 0;
		virtual timeType fGetLastCommunicationTime() const {
			return lLastCommunicationTime;
		};
		virtual timeType fGetReadStartTime() const {
			return lReadStartTime;
		};
	};
	class serialLine : public communicationChannel {

	  public:
		enum Parity {
			NONE = 0,
			ODD = 1,
			EVEN = 2
		};
	  protected:
		std::string lDevName;
		int lBaudRate;
		Parity lParity;
		int lBits;
		int lStopBits;
		int lRetries;
		char lSeparator;
		bool lReconnect;
	  public:
		serialLine(const std::string& aDevName, int aBaudRate, Parity aParity = NONE, int aBits = 8, int aStopBits = 1);
		virtual ~serialLine();
		virtual void fSetLineSeperator(char aSeperator);
		virtual void fSetRetries(int aRetries);
		virtual bool fInit();
		virtual int fRead(char *aBuffer, int aBuffsize, durationType aTimeout);
		virtual bool fWrite(const char *Data);
		virtual int fFlushReceiveBuffer();
		virtual int fFlushTransmitBuffer();
		virtual void fSetReconnectOnError();
	};
} // end namespace slowcontrol

#endif
