#include <stdio.h>
#include <stdlib.h>
#include <linux/i2c.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <Options.h>
#include <iostream>
#include <array>
/* Puffer fuer die RTC-Daten */
#define BUFSIZE 7

/* Verwende I2C 1 */
#define I2CADDR "/dev/i2c-1"

/* defined in <linux/i2c-dev.h> */
#define I2C_SLAVE 0x703

class channelPair {
  protected:
	int fd;
	unsigned char ls0;
	void setRegister(unsigned char reg, unsigned char value) {
		//ioctl(fd, I2C_SLAVE, addr);
		unsigned char buf[2] = {reg, value};
		write(fd, buf, 2);
	}

  public:
	channelPair(const std::string& i2cDevname, int aAddr) {
		fd = open(i2cDevname.c_str(), O_RDWR);
		ioctl(fd, I2C_SLAVE, aAddr);
		setRegister(5, 5); // switch off light
		setRegister(1, 0); // pwm0 to full speed
		setRegister(3, 0); // pwm0 to full speed
	};
	void set(int channel, float value) {
		if (value == 0.0) { // set to off
			ls0 = (ls0 & (channel ? 0x0C : 0x03)) | (channel ? 0x01 : 0x04);
			setRegister(5, ls0);
		} else {
			ls0 = (ls0 & (channel ? 0x0C : 0x03)) | (channel ? 0x0C : 0x02);
			setRegister(5, ls0);
			unsigned char pwm = 255 - value * 255;
			setRegister(channel ? 4 : 2, pwm);
		}
	}

};

class lightChannel: public slowcontrol::measurement<float>, public slowcontrol::writeValueWithType<float> {
  protected:
	channelPair& pair;
	int channel;
  public:
	lightChannel(const std::string& baseName,
	             const std::string& colour,
	             channelPair& aPair,
	             int aChannel) : measurement(0), pair(aPair), channel(aChannel) {
		fInitializeUid(baseName + colour);
		fConfigure();
	}
	bool fProcessRequest(const request* aRequest, std::string& aResponse) override {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			pair.set(channel, req->lGoalValue);
			aResponse = "done";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	}
};


class rgbw {
  protected:
	channelPair pair0;
	channelPair pair1;
	lightChannel red;
	lightChannel green;
	lightChannel blue;
	lightChannel white;
  public:
	rgbw(const std::string& i2cDevname,
	     const std::string& nameBase):
		pair0(i2cDevname, 0x62),
		pair1(i2cDevname, 0x63),
		red(nameBase, "red", pair1, 0),
		green(nameBase, "green", pair0, 1),
		blue(nameBase, "blue", pair1, 1),
		white(nameBase, "white", pair0, 0) {
	}
};


int main(int argc, const char *argv[]) {
	options::single<std::string> deviceName('d', "device", "name of the i2c device", "/dev/i2c-1");
	options::single<std::string> baseName('n', "name", "name base of the controls");
	options::parser parser("slowcontrol program for controlling rgbw lights");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("rgbwPCA9533d");

	rgbw conroller(deviceName, baseName);

	daemon->fStartThreads();
	daemon->fWaitForThreads();
}
