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
	channelPair(const std::string& i2cDevname, int aAddr):
		ls0(0x05) {
		fd = open(i2cDevname.c_str(), O_RDWR);
		ioctl(fd, I2C_SLAVE, aAddr);
		setRegister(5, ls0); // switch off light
		setRegister(1, 0); // pwm0 to full speed
		setRegister(3, 0); // pwm0 to full speed
	};
	void set(int channel, float value) {
		if (value == 0.0) { // set to off
			ls0 = (ls0 & (channel ? 0x03 : 0x0C)) | (channel ? 0x04 : 0x01);
			setRegister(5, ls0);
		} if (value == 1.0) {
			ls0 = (ls0 & (channel ? 0x03 : 0x0C));
			setRegister(5, ls0);
		}else {
			ls0 = (ls0 & (channel ? 0x03 : 0x0C)) | (channel ? 0x0C : 0x02);
			setRegister(5, ls0);
			unsigned char pwm = 255 - value * 255;
			setRegister(channel ? 4 : 2, pwm);
		}
	}

};

class rgbw;

class lightChannel: public slowcontrol::measurement<float>, public slowcontrol::writeValueWithType<float> {
  protected:
	rgbw& master;
	channelPair& pair;
	int channel;
  public:
	lightChannel(rgbw& aMaster,
							 const std::string& baseName,
	             const std::string& colour,
	             channelPair& aPair,
	             int aChannel) : measurement(0), master(aMaster), pair(aPair), channel(aChannel) {
		fInitializeUid(baseName + colour);
		fConfigure();
	}
	void set(float aValue, bool recalc = true);
	bool fProcessRequest(const request* aRequest, std::string& aResponse) override {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			set(req->lGoalValue);
			aResponse = "done";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	}
};

class modelChannel: public slowcontrol::measurement<float>, public slowcontrol::writeValueWithType<float> {
  protected:
	rgbw& master;
  public:
	modelChannel(rgbw& aMaster,
							 const std::string& baseName,
	             const std::string& channelName) : measurement(0), master(aMaster) {
		fInitializeUid(baseName + channelName);
		fConfigure();
	}
	void set(float aValue, bool recalc = true);
	bool fProcessRequest(const request* aRequest, std::string& aResponse) override {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			set(req->lGoalValue);
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
	modelChannel hue;
	modelChannel value;
	modelChannel saturation;
  public:
	rgbw(const std::string& i2cDevname,
	     const std::string& nameBase):
		pair0(i2cDevname, 0x62),
		pair1(i2cDevname, 0x63),
		red(*this,nameBase, "red", pair1, 0),
		green(*this,nameBase, "green", pair0, 1),
		blue(*this,nameBase, "blue", pair1, 1),
		white(*this,nameBase, "white", pair0, 0),
		hue(*this,nameBase, "hue"),
		value(*this,nameBase, "value"),
	  saturation(*this,nameBase, "saturation")
	{
		std::string compoundName(nameBase);
		compoundName += "light";
		
		auto compound = slowcontrol::base::fGetCompoundId(compoundName.c_str());
		slowcontrol::base::fAddToCompound(compound,red.fGetUid(), "red");
		slowcontrol::base::fAddToCompound(compound,green.fGetUid(), "green");
		slowcontrol::base::fAddToCompound(compound,blue.fGetUid(), "blue");
		slowcontrol::base::fAddToCompound(compound,white.fGetUid(), "white");
		slowcontrol::base::fAddToCompound(compound,hue.fGetUid(), "hue");
		slowcontrol::base::fAddToCompound(compound,saturation.fGetUid(), "saturation");
		slowcontrol::base::fAddToCompound(compound,value.fGetUid(), "value");
	}
	void hsvToRgb() {
		auto h=hue.fGetCurrentValue();
		auto s=saturation.fGetCurrentValue();
		auto v=value.fGetCurrentValue();
		if( s == 0 ) { // achromatisch (Grau)
			if (v < 0.5) {
				white.set(v*2,false);
				red.set(0,false);
				green.set(0,false);
				blue.set(0,false);
			} else {
				white.set(1, false);
				red.set((v-0.5)*2,false);
				green.set((v-0.5)*2,false);
				blue.set((v-0.5)*2,false);
			}
      return;
   }
   h /= 60;           // sector 0 to 5
   int i = floor( h );
   float f = h - i;         // factorial part of h
   float p = v * ( 1 - s );
   float q = v * ( 1 - s * f );
   float t = v * ( 1 - s * ( 1 - f ) );
	 float r,g,b;
   switch( i ) {
	 case 0: r = v; g = t; b = p; break;
	 case 1: r = q; g = v; b = p; break;
	 case 2: r = p; g = v; b = t; break;
	 case 3: r = p; g = q; b = v; break;
	 case 4: r = t; g = p; b = v; break;
	 default:  // case 5:
		 r = v; g = p; b = q; break;
   }
	 auto min = std::min(r,std::min(g,b));
	 if (min > 0.5) {
		 min = 0.5;
	 }
	 white.set(2*min,false);
	 red.set(2*(r-min), false);
	 green.set(2*(g-min), false);
	 blue.set(2*(b-min), false);
	}
	void rgbToHsv() {
		auto w=white.fGetCurrentValue() * 0.5;
		auto r=red.fGetCurrentValue() * 0.5 + w;
		auto g=green.fGetCurrentValue() *0.5 + w;
		auto b=blue.fGetCurrentValue() * 0.5 +w;
		float min, max, delta;
		min = std::min(r, std::min(g, b ));
		max = std::max(r, std::max(g, b ));
		value.set(max, false);
		delta = max - min;
		if( max != 0 ) {
			saturation.set(delta / max, false);
		} else {                           // r = g = b = 0
      saturation.set(0.0, false);
			hue.set(0.0, false);
			return;
   }
   if (max == min) {                // hier ist alles Grau
		 hue.set(0.0,false);
		 saturation.set(0.0,false);
		 return;
   }
	 float h;
   if( r == max ) {
		 h = ( g - b ) / delta;       // zwischen Gelb und Magenta
	 } else if( g == max ) {
     h = 2 + ( b - r ) / delta;   // zwischen Cyan und Gelb
   } else {
		 h = 4 + ( r - g ) / delta;   // zwischen Magenta und Zyan
	 }
	 h *= 60;                     // degrees
   if( h < 0 ) {
      h += 360;
	 }
	 hue.set(h,false);
	};
};


void lightChannel::set(float aValue, bool recalc) {
	pair.set(channel, aValue);
	fStore(aValue);
	fFlush(true);
	if (recalc) {
		master.rgbToHsv();
	}
}
void modelChannel::set(float aValue, bool recalc) {
	fStore(aValue);
	fFlush(true);
	if (recalc) {
		master.hsvToRgb();
	}
}

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
