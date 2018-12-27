#include "communications.h"
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

namespace slowcontrol {
	communicationChannel::communicationChannel() :
		lThrowLevel(exception::level::kNone) {
	};

	bool communicationChannel::fWrite(const char *aData) {
		auto datasize = strlen(aData);
		auto written = ::write(lFd, aData, datasize);
		if (written < 0) {
			if (lThrowLevel > exception::level::kNone) {
				throw exception(std::strerror(errno), lThrowLevel);
			}
			syslog(LOG_WARNING, "could not write %zu bytes to socket %d: %m", datasize, lFd);
		}
		return (written == static_cast<decltype(written)>(datasize));
	}

	serialLine::serialLine(const std::string& aDevName, int aBaudRate, Parity aParity, int aBits, int aStopBits) :
		communicationChannel(),
		lDevName(aDevName),
		lBaudRate(aBaudRate),
		lParity(aParity),
		lBits(aBits),
		lStopBits(aStopBits),
		lRetries(10),
		lSeparator('\n'),
		lReconnect(false) {
		fInit();
	}
	serialLine::~serialLine() {
		close(lFd);
	}
	void serialLine::fSetLineSeparator(char aSeparator) {
		lSeparator = aSeparator;
	}

	/// set the number of retries for reading, default is 10
	/// set to 0 for spontaneous data, where timeouts are not errors
	void serialLine::fSetRetries(int aRetries) {
		lRetries = aRetries;
	}

	void serialLine::fSetReconnectOnError() {
		lReconnect = true;
	}
	bool serialLine::fInit() {
		lFd = open(lDevName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
		if (lFd == -1) {
			return false;
		}
		struct termios fd_attr;
		fd_attr.c_iflag = IGNBRK;
		fd_attr.c_iflag &= ~(INLCR | IGNCR | ICRNL);
		fd_attr.c_oflag = 0;
		fd_attr.c_cflag = CREAD		/* enable receiver */
		                  | CLOCAL;	/* ignore modem control lines */
		switch (lBits) {
			case 8:
				fd_attr.c_cflag |= CS8;
				break;	/* eight bits per character */
			case 7:
				fd_attr.c_cflag |= CS7;
				break;
			case 6:
				fd_attr.c_cflag |= CS6;
				break;
			case 5:
				fd_attr.c_cflag |= CS5;
				break;
			default:
				break;
		}
		if (lParity == ODD) {
			fd_attr.c_cflag |= PARENB | PARODD;
		} else if (lParity == EVEN) {
			fd_attr.c_cflag |= PARENB;
		}
		if (lStopBits == 2) {
			fd_attr.c_cflag |= CSTOPB;
		}
		fd_attr.c_lflag = 0;
		speed_t baud;
		switch (lBaudRate) {
			case 0:
				baud = B0;
				break;
			case 50:
				baud = B50;
				break;
			case 75:
				baud = B75;
				break;
			case 110:
				baud = B110;
				break;
			case 134:
				baud = B134;
				break;
			case 150:
				baud = B150;
				break;
			case 200:
				baud = B200;
				break;
			case 300:
				baud = B300;
				break;
			case 600:
				baud = B600;
				break;
			case 1200:
				baud = B1200;
				break;
			case 1800:
				baud = B1800;
				break;
			case 2400:
				baud = B2400;
				break;
			case 4800:
				baud = B4800;
				break;
			case 9600:
				baud = B9600;
				break;
			case 19200:
				baud = B19200;
				break;
			case 38400:
				baud = B38400;
				break;
			case 57600:
				baud = B57600;
				break;
			case 115200:
				baud = B115200;
				break;
			case 230400:
				baud = B230400;
				break;
			case 500000:
				baud = B500000;
				break;
			case 576000:
				baud = B576000;
				break;
			case 921600:
				baud = B921600;
				break;
			case 1000000:
				baud = B1000000;
				break;
			case 1152000:
				baud = B1152000;
				break;
			case 1500000:
				baud = B1500000;
				break;
			case 2000000:
				baud = B2000000;
				break;
			case 2500000:
				baud = B2500000;
				break;
			case 3000000:
				baud = B3000000;
				break;
			case 3500000:
				baud = B3500000;
				break;
			case 4000000:
				baud = B4000000;
				break;
			default:
				exit(EXIT_FAILURE);
		}
		cfsetospeed(&fd_attr, baud);
		cfsetispeed(&fd_attr, baud);
		if (tcsetattr(lFd, TCSANOW, &fd_attr) == -1) { /* commit changes NOW */
			close(lFd);
			exit(1);
		}
		int flags;
		flags = fcntl(lFd, F_GETFL);
		flags &= ~O_NONBLOCK;
		if (fcntl(lFd, F_SETFL, flags) != 0) {
			syslog(LOG_ERR, "error changing to blocking mode: %m");
		}
		return true;
	}

	int serialLine::fRead(char *aBuffer, int aBuffsize, durationType aTimeout) {
		char* replyptr = aBuffer;
		struct pollfd pollfds[1];
		pollfds[0].fd = lFd;
		pollfds[0].events = POLLIN | POLLERR;
		int charsread = 0;
		lReadStartTime = timeType::min();
		for (int timeouts = 0; charsread + 2 < aBuffsize;) {
			poll(pollfds, sizeof(pollfds) / sizeof(pollfds[0]), std::chrono::duration_cast<std::chrono::milliseconds>(aTimeout).count());
			if (pollfds[0].revents & POLLERR) { /* some problem occurred */
				if (lThrowLevel > exception::level::kNone) {
					throw exception(std::strerror(errno), lThrowLevel);
				}
				syslog(LOG_ERR, "error while polling tty. Interface gone?");
				if (lReconnect) {
					close(lFd);
					fInit();
					pollfds[0].fd = lFd;
					continue;
				}
			}
			if (pollfds[0].revents & POLLIN) { /* data from tty */
				auto now = std::chrono::system_clock::now();
				if (lReadStartTime == timeType::min()) {
					lReadStartTime = now;
				}
				if (now > lLastCommunicationTime) {
					lLastCommunicationTime = now;
				}
				char c;
				if (unlikely(::read(lFd, &c, 1) != 1)) {
					if (lThrowLevel > exception::level::kNone) {
						throw exception(std::strerror(errno), lThrowLevel);
					}
					syslog(LOG_ERR, "error reading from tty");
				}
				if (c == lSeparator) {
					break;
				} else {
					*replyptr++ = c;
					charsread++;
				}
			} else {
				if (lRetries > 0) {
					syslog(LOG_WARNING, "timeout waiting for data");
					timeouts++;
					if (timeouts > lRetries) {
						if (lReconnect) {
							close(lFd);
							fInit();
							pollfds[0].fd = lFd;
							return -2;
						}
						syslog(LOG_ERR, "too many timeouts waiting for data");
						return -1;
					}
				} else {
					return -1;
				}
			}
		}
		*replyptr++ = '\n';
		*replyptr = '\0';
		return charsread;
	}

	bool serialLine::fWrite(const char *aData) {
		auto retval = communicationChannel::fWrite(aData);
		auto startTime = std::chrono::system_clock::now();;
		if (lLastCommunicationTime > startTime) {// the last write is not yet finmished
			startTime = lLastCommunicationTime;
		}
		lLastCommunicationTime = startTime + std::chrono::nanoseconds((strlen(aData) * (lBits + lStopBits + (lParity == NONE ? 0 : 1)) / lBaudRate) * 1000000000); // consider extra time for realy sending the data, icluding the two terminators
		return retval;
	}

	int serialLine::fFlushReceiveBuffer() {
		return tcflush(lFd, TCIFLUSH);
	}

	int serialLine::fFlushTransmitBuffer() {
		return tcflush(lFd, TCOFLUSH);
	}

} // end namespace slowcontrol
