#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/i2c-dev.h>

#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <Options.h>
#include <iostream>
#include <array>
#include <curl/curl.h>
#include <mutex>
#include <condition_variable>
		

static options::single<std::string> url('u', "url", "url to post request to"); 

class remotePCA9685 {
  protected:
	std::mutex pcaMutex;
	std::condition_variable condVar;
	CURL *curl;
	curl_mime *mime;
	unsigned int nMimeParts;
	std::array<unsigned short,16> values;
	std::thread senderThread;
public:
	void senderFunction() {
		std::cerr << "this is " << (void*) this << "\n";
		setRegister(1,4);
		sendRequest();
		std::array<unsigned short,16> valuesRemote;
		while(true) {
			std::array<unsigned short,16> valuesNew;
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
			{
				std::unique_lock<decltype(pcaMutex)> lock;
				//	std::cerr << __LINE__ << "this is " << (void*) this << "\n";
				//std::cerr << __LINE__ << "this is " << (void*) &lock << "\n";
				//std::cerr << __LINE__ << "this is " << (void*) &condVar << "\n";
				//condVar.wait(lock);
				if (values == valuesRemote) {
					continue; // nothing to do
				}
				valuesNew = values;
			}
			auto nPulsing = std::count_if(valuesNew.cbegin(), valuesNew.cend(), [](unsigned short v){return v > 0 && v<4096;});
			int nPulsingSoFar=0;
			unsigned short totalPulse=0;
			for (unsigned int i=0; i<valuesNew.size(); i++) {
				if (valuesNew.at(i) == valuesRemote.at(i)) { // nothing to do
					if (valuesNew.at(i) > 0 && valuesNew.at(i) < 4096) {
						nPulsingSoFar++;
						totalPulse+=valuesNew.at(i);
					}
					continue;
				}
				if (valuesNew.at(i) == 0) {
					setRegister(7+4*i, 0x00);
					setRegister(9+4*i, 0x10);
				} else if (valuesNew.at(i) == 4096) {
					setRegister(7+4*i, 0x10);
					setRegister(9+4*i, 0x00);
				} else {
					unsigned short start;
					if (totalPulse + valuesNew.at(i) < 4096) {
						start = totalPulse;
					} else {
						start = 0;
					}
					unsigned short stop=start + valuesNew.at(i);
					setRegister(6+4*i,start & 0xffu);
					setRegister(7+4*i,(start >> 8) & 0x0fu);
					setRegister(8+4*i,stop & 0xffu);
					setRegister(9+4*i,(stop >> 8) & 0x0fu);
					totalPulse+=valuesNew.at(i);
				}
				valuesRemote.at(i) = valuesNew.at(i);
			}
			sendRequest();
		}
	}

	
	void setRegister(unsigned char reg, unsigned char value) {
		if (mime == nullptr) {
			mime = curl_mime_init(curl);
		}
		auto *part = curl_mime_addpart(mime);
		std::string name("reg");
		name += std::to_string(reg);
		std::string v(std::to_string(value));
		curl_mime_data(part, v.c_str(), CURL_ZERO_TERMINATED);
		curl_mime_name(part, name.c_str());
		std::cerr << name << " : " << v << "\n";
		if (nMimeParts > 16) {
			sendRequest();
		}
	}
public:
	remotePCA9685(): nMimeParts(0) {
		curl = curl_easy_init();
		mime = nullptr;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		std::cerr << "url: " << url << "\n";
	}
	void startThread() {
		senderThread = std::move(std::thread(&remotePCA9685::senderFunction,this));
	}
	void act() {
		std::lock_guard<decltype(pcaMutex)> lock(pcaMutex);
		condVar.notify_one();
	}
	void sendRequest(){
		auto now = std::chrono::system_clock::now();
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
		char curlErrorBuffer[CURL_ERROR_SIZE];
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuffer);
		auto result = curl_easy_perform(curl);
		auto dt = std::chrono::system_clock::now() - now;
		if (result != CURLE_OK) {
			std::cerr << "curled a " << curlErrorBuffer << "\n";
		} else {
			std::cerr << "fine after " << std::chrono::duration_cast<std::chrono::duration<float>>(dt).count() << "s\n";
		}
		curl_mime_free(mime);
		nMimeParts = 0;
		mime = nullptr;
	}
	void set(int channel, float value) {
		std::lock_guard<decltype(pcaMutex)> lock(pcaMutex);
		if (value < 1.0 / 4096) { // set to off
			values.at(channel)=0;
		} else if (value > 1.0 - 1.0 / 4096 ) {
			values.at(channel)=4096;
		} else {
			values.at(channel)=value * 4096;
		}
	}
};
//std::mutex remotePCA9685::pcaMutex;
class rgbw;

class setableChannel {
  protected:
  protected:
	rgbw& master;
	std::string name;
  public:
	setableChannel(rgbw& aMaster, const std::string& aName):
		master(aMaster),
		name(aName) {};
	virtual void set(float aValue, bool recalc = true) = 0;
	virtual float getValue() const = 0;
	virtual float getMax() const {
		return 1.0;
	};
	const std::string& getName() const {
		return name;
	};
};

class channelBase: public setableChannel,
	public slowcontrol::measurement<float>,
	public slowcontrol::writeValueWithType<float> {
  protected:
	const float maxValue;
  public:
	channelBase(rgbw& aMaster, float aMax, const std::string& aName): setableChannel(aMaster, aName),
		measurement(0),
		maxValue(aMax) {};
	float getMax() const override {
		return maxValue;
	};
	float getValue() const override {
		return fGetCurrentValue();
	}
};


class lightChannel: public channelBase {
  protected:
	int channel;
  public:
	lightChannel(rgbw& aMaster,
	             const std::string& baseName,
	             const std::string& colour,
	             int aChannel) : channelBase(aMaster, 1.0, colour),
		channel(aChannel) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(baseName + colour);
		fConfigure();
		set(0,false);
	}
	void set(float aValue, bool recalc = true) override;
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

class whiteChannel: public lightChannel {
protected:
	int channel2;
public:
	whiteChannel(rgbw& aMaster,
	             const std::string& baseName,
	             const std::string& colour,
	             int aChannel,
							 int aChannel2): lightChannel(aMaster,baseName,colour,aChannel),
															 channel2(aChannel2) {
	}
	void set(float aValue, bool recalc = true) override;
};



class modelChannel: public channelBase {
  public:
	modelChannel(rgbw& aMaster,
	             const std::string& baseName,
	             const std::string& channelName,
	             float aMax = 1.0) : channelBase(aMaster, aMax, channelName) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(baseName + channelName);
		fConfigure();
	}
	void set(float aValue, bool recalc = true) override;
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

class setbit: public setableChannel,
	public slowcontrol::measurement<bool>,
	public slowcontrol::writeValueWithType<bool> {
  protected:
	bool value;
	int fd;
	bool fProcessRequest(const writeValue::request* aRequest, std::string& aResponse) override {
		auto req = dynamic_cast<const requestWithType*>(aRequest);
		if (req != nullptr) {
			fSet(req->lGoalValue);
			aResponse = "done";
			return true;
		}
		aResponse = "can't cast request";
		return false;
	}

	void fSet(bool aValue) {
		value = aValue;
		fStore(value);
		char buf = value ? '1' : '0';
		pwrite(fd, &buf, 1, 0);
	}
  public:
	setbit(rgbw& aMaster, const std::string& baseName, const std::string& aName, int indicatorPin):
		setableChannel(aMaster, aName) {
		lClassName.fSetFromString(__func__);
		fInitializeUid(baseName + aName);
		fConfigure();
		{
			std::ofstream exporter("/sys/class/gpio/export");
			exporter << indicatorPin << "\n";
		}
		std::string path("/sys/class/gpio/gpio");
		path += std::to_string(indicatorPin);
		{
			std::ofstream director(path + "/direction");
			director << "out\n";
		}
		path += "/value";
		fd = open(path.c_str(), O_WRONLY);
		if (fd < 0) {
			throw slowcontrol::exception("can't open gpio", slowcontrol::exception::level::kStop);
		}
	}
	float getValue() const override {
		if (value) {
			return 1;
		}
		return 0;
	}
	void set(float aValue, bool recalc = true) override {
		fSet(aValue > getValue());
	};
	operator bool() const {
		return value;
	}
};



class rgbw {
protected:
  public:
	remotePCA9685 pca9685;
	lightChannel red;
	lightChannel green;
	lightChannel blue;
	lightChannel yellow;
	whiteChannel white;
	modelChannel hue;
	modelChannel value;
	modelChannel saturation;
	setbit autoHue;
	rgbw(const std::string& i2cDevname,
	     const std::string& nameBase):
		red(*this, nameBase, "red", 1),
		green(*this, nameBase, "green", 2),
		blue(*this, nameBase, "blue", 0),
		yellow(*this, nameBase, "yellow", 3),
		white(*this, nameBase, "white", 4,5),
		hue(*this, nameBase, "hue", 360.0),
		value(*this, nameBase, "value"),
		saturation(*this, nameBase, "saturation"),
		autoHue(*this, nameBase, "autoHue", 4) {
		std::string compoundName(nameBase);
		compoundName += "light";

		auto compound = slowcontrol::base::fGetCompoundId(compoundName.c_str());
		slowcontrol::base::fAddToCompound(compound, red.fGetUid(), "red");
		slowcontrol::base::fAddToCompound(compound, green.fGetUid(), "green");
		slowcontrol::base::fAddToCompound(compound, blue.fGetUid(), "blue");
		slowcontrol::base::fAddToCompound(compound, blue.fGetUid(), "yellow");
		slowcontrol::base::fAddToCompound(compound, white.fGetUid(), "white");
		slowcontrol::base::fAddToCompound(compound, hue.fGetUid(), "hue");
		slowcontrol::base::fAddToCompound(compound, saturation.fGetUid(), "saturation");
		slowcontrol::base::fAddToCompound(compound, value.fGetUid(), "value");
		slowcontrol::base::fAddToCompound(compound, autoHue.fGetUid(), "autoHue");
	}

	void set(int channel, float val) {
		pca9685.set(channel, val);
	}
	void act() {
		pca9685.act();
	}

	setableChannel& getChannel(bool r, bool g, bool b) {
		unsigned bits = (r ? 1 : 0 ) | (g ? 2 : 0) | (b ? 4 : 0);
		switch (bits) {
			case 0:
				return value;
			case 1: // red
				return red;
			case 2: // green
				return green;
			case 3: // red and green
				return hue;
			case 4: // blue
				return blue;
			case 5: // red and blue
				return saturation;
			case 6: // green and blue
				return autoHue;
			case 7: // red, green and blue
				return white;
		}
		return value;
	}

	void hsvToRgb() {
		std::cerr << __func__ << "\n";
		auto h = hue.fGetCurrentValue();
		auto s = saturation.fGetCurrentValue();
		auto v = value.fGetCurrentValue();
		if ( s == 0 ) { // achromatisch (Grau)
			if (v < 0.5) {
				white.set(v * 2, false);
				red.set(0, false);
				green.set(0, false);
				blue.set(0, false);
				yellow.set(0, false);
			} else {
				white.set(1, false);
				//	yellow.set((v - 0.5) * 2, false);
				red.set((v - 0.5) * 2, false);
				green.set((v - 0.5) * 2, false);
				blue.set((v - 0.5) * 2, false);
			}
			return;
		}
		h /= 60;           // sector 0 to 5
		int i = floor( h );
		float f = h - i;         // factorial part of h
		float p = v * ( 1 - s );
		float q = v * ( 1 - s * f );
		float t = v * ( 1 - s * ( 1 - f ) );
		float r, g, b;
		switch ( i ) {
			case 0:
				r = v;
				g = t;
				b = p;
				break;
			case 1:
				r = q;
				g = v;
				b = p;
				break;
			case 2:
				r = p;
				g = v;
				b = t;
				break;
			case 3:
				r = p;
				g = q;
				b = v;
				break;
			case 4:
				r = t;
				g = p;
				b = v;
				break;
			default:  // case 5:
				r = v;
				g = p;
				b = q;
				break;
		}
		auto min = std::min(b, std::min(r,g));
		if (min > 0.5) {
			min = 0.5;
		}
		r -= min;
		g -= min;
		b -= min;
		auto rgmin = std::min(r,g);
		yellow.set(2*rgmin, false);
		r -= rgmin;
		g -= rgmin;
		white.set(2 * min, false);
		red.set(2 * r, false);
		green.set(2 * g, false);
		blue.set(2 * b, false);
	}
	void rgbToHsv() {
		std::cerr << __func__ << "\n";
		auto w = white.fGetCurrentValue() * 0.5;
		auto y = yellow.fGetCurrentValue() *0.5;
		auto r = std::min(red.fGetCurrentValue() * 0.5 + w + y, 1.0);
		auto g = std::min(green.fGetCurrentValue() * 0.5 + w + y, 1.0);
		auto b = std::min(blue.fGetCurrentValue() * 0.5 + w, 1.0);
		float min, max, delta;
		min = std::min(r, std::min(g, b ));
		max = std::max(r, std::max(g, b ));
		value.set(max, false);
		delta = max - min;
		if ( max != 0 ) {
			saturation.set(delta / max, false);
		} else {                           // r = g = b = 0
			saturation.set(0.0, false);
			hue.set(0.0, false);
			std::cerr << __LINE__ << " w: " << w << " y: " << y << " r: " << r << " g: " << g << " b: " << b
								<< " v: " << value.fGetCurrentValue() 
								<< " h: " << hue.fGetCurrentValue() 
								<< " s: " << saturation.fGetCurrentValue() << "\n"; 
			return;
		}
		if (max == min) {                // hier ist alles Grau
			hue.set(0.0, false);
			saturation.set(0.0, false);
			std::cerr << __LINE__ << " w: " << w << " y: " << y << " r: " << r << " g: " << g << " b: " << b
								<< " v: " << value.fGetCurrentValue() 
								<< " h: " << hue.fGetCurrentValue() 
								<< " s: " << saturation.fGetCurrentValue() << "\n"; 
			return;
		}
		float h;
		if ( r == max ) {
			h = ( g - b ) / delta;       // zwischen Gelb und Magenta
		} else if ( g == max ) {
			h = 2 + ( b - r ) / delta;   // zwischen Cyan und Gelb
		} else {
			h = 4 + ( r - g ) / delta;   // zwischen Magenta und Zyan
		}
		h *= 60;                     // degrees
		if ( h < 0 ) {
			h += 360;
		}
		hue.set(h, false);
	std::cerr << __LINE__ << " w: " << w << " y: " << y << " r: " << r << " g: " << g << " b: " << b
								<< " v: " << value.fGetCurrentValue() 
								<< " h: " << hue.fGetCurrentValue() 
								<< " s: " << saturation.fGetCurrentValue() << "\n"; 
	};
};


void lightChannel::set(float aValue, bool recalc) {
	aValue = std::max(aValue, 0.0f);
	aValue = std::min(aValue, 1.0f);
	master.set(channel, aValue);
	fStore(aValue);
	if (recalc) {
		master.act();
		master.rgbToHsv();
	}
}
void whiteChannel::set(float aValue, bool recalc) {
	aValue = std::max(aValue, 0.0f);
	aValue = std::min(aValue, 1.0f);
	master.set(channel, aValue);
	master.set(channel2, aValue);
	fStore(aValue);
	if (recalc) {
		master.act();
		master.rgbToHsv();
	}
}
void modelChannel::set(float aValue, bool recalc) {
	aValue = std::max(aValue, 0.0f);
	aValue = std::min(aValue, maxValue);
	fStore(aValue);
	if (recalc) {
		master.hsvToRgb();
		master.act();
	}
}



class stateButton {
  protected:
	bool value;
	int fd;
	std::string pathBase;
  public:
	stateButton(unsigned int pin): pathBase("/sys/class/gpio/gpio") {
		{
			std::ofstream exporter("/sys/class/gpio/export");
			exporter << pin << "\n";
		}
		pathBase += std::to_string(pin);
		{
			std::ofstream director(pathBase + "/direction");
			director << "in\n";
		}
		std::string path(pathBase);
		path += "/value";
		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			throw slowcontrol::exception("can't open gpio", slowcontrol::exception::level::kStop);
		}
	}
	void update() {
		//		lseek(fd, 0, SEEK_SET);
		char buffer;
		if (pread(fd, &buffer, 1, 0) < 1) {
			throw slowcontrol::exception("can't read gpio", slowcontrol::exception::level::kContinue);
		}
		value = buffer == '0'; // we are active low...
	}
	operator bool() const {
		return value;
	}
	const std::string& getName() const {
		return pathBase;
	}
};

class pushButton: public stateButton {
  public:
	pushButton(unsigned int pin): stateButton(pin) {
		{
			std::ofstream director(pathBase + "/edge");
			director << "both\n";
		}
		std::cerr << "pin " << pin << " fd " << fd << "\n";
	}
	int getFd() const {
		return fd;
	}
};

class rotary {
  protected:
	pushButton& a;
	pushButton& b;
  public:
	rotary(pushButton& A, pushButton& B): a(A), b(B) {};
	int getFdA() const {
		return a.getFd();
	};
	int getFdB() const {
		return b.getFd();
	};
	float getIncrement(int pushFd, float step) {
		if (pushFd == a.getFd()) {
			std::cerr << "trig a " << a.getName() << ": " << a << " " << b.getName() << ": " << b << "\n";
			if (a) { // rising edge
				if (b) { // ccw
					return -step;
				} else { // cw
					return step;
				}
			} else {
				if (b) { // cw
					return step;
				} else { // ccw
					return -step;
				}
			}
		} else {
			std::cerr << "trig b " << a.getName() << ": " << a << " " << b.getName() << ": " << b << "\n";

			if (b) { // riding edge
				if (a) { // cw
					return step;
				} else { // ccw
					return -step;
				}
			} else {
				if (a) { // ccw
					return -step;
				} else { // cw
					return step;
				}
			}
		}
	}
};


int main(int argc, const char *argv[]) {
	options::single<std::string> deviceName('d', "device", "name of the i2c device", "/dev/i2c-1");
	options::single<std::string> baseName('n', "name", "name base of the controls");
	options::parser parser("slowcontrol program for controlling rgbw lights");
	parser.fParse(argc, argv);



	auto daemon = new slowcontrol::daemon("rgbwPCA9533d");

	rgbw controller(deviceName, baseName);

	daemon->fStartThreads();
	std::thread senderThread(&remotePCA9685::senderFunction, &controller.pca9685);
	std::vector<stateButton> buttons({17, 27, 22});
	auto& redButton = buttons.at(0);
	auto& greenButton = buttons.at(1);
	auto& blueButton = buttons.at(2);
	std::vector<pushButton> push({13, 6, 5, 26, 19});
	auto& onOff = push.at(0);
	std::vector<rotary> rot({{push.at(1), push.at(2)}, {push.at(3), push.at(4)}});
	std::map<int, rotary&> fdRotMap;
	for (auto& r : rot) {
		fdRotMap.emplace(r.getFdA(), r);
		fdRotMap.emplace(r.getFdB(), r);
	}
	std::vector<struct pollfd> pfds(push.size());
	for (unsigned int i = 0; i < push.size(); i++) {
		pfds.at(i).fd = push.at(i).getFd();
		pfds.at(i).events = POLLPRI | POLLERR;
		std::cerr << pfds.at(i).fd << "\n";
	}
	float oldValue = 1;
	auto lastAutoHueSet = std::chrono::system_clock::now();
	auto lastRotTick = lastAutoHueSet;
	bool wasIncreasing = true;
	while (!daemon->fGetStopRequested()) {
		auto result = poll(pfds.data(), pfds.size(), 1000);
		if (result > 0) {
			std::cerr << "result " << result << " ";
			for (auto& pfd : pfds) {
				std::cerr << pfd.fd << ": ";
				if (pfd.revents & POLLIN) {
					std::cerr << "IN|";
				}
				if (pfd.revents & POLLPRI) {
					std::cerr << "PRI|";
				}
				if (pfd.revents & POLLOUT) {
					std::cerr << "OUT|";
				}
				if (pfd.revents & POLLERR) {
					std::cerr << "ERR|";
				}
				if (pfd.revents & POLLNVAL) {
					std::cerr << "NVAL|";
				}
			}
			std::cerr << "\tc";
			std::this_thread::sleep_for(std::chrono::milliseconds(2)); // stabilize input
			for (auto& b : buttons) {
				b.update();
				std::cerr << b << " ";
			}
			bool something = false;
			for (auto& b : push) {
				b.update();
				something |= b;
				std::cerr << b << " ";
			}
			std::cerr << "\n";
			if (!something) {
				continue;
			}
			if (pfds.at(0).revents != 0) {
				if (onOff) {
					std::cerr << "onOff pressed\n";
					if (controller.value.fGetCurrentValue() > 0.0) {
						oldValue = controller.value.fGetCurrentValue();
						controller.value.set(0);
					} else {
						controller.value.set(oldValue);
					}
				} else {
					std::cerr << "onOff relased\n";
				}
				pfds.at(0).revents = 0;
			}
			auto& channel = controller.getChannel(redButton, greenButton, blueButton);

			for (auto& pfd : pfds) {
				if (pfd.fd == pfds.at(0).fd) {
					continue;
				}
				if (pfd.revents != 0) {
					auto now = std::chrono::system_clock::now();
					auto dt = now - lastRotTick;
					std::cerr << "dt is " << std::chrono::duration_cast<std::chrono::duration<float>>(dt).count() << "\n";
					if (dt > std::chrono::milliseconds(10)) {
						lastRotTick = now;
						auto it = fdRotMap.find(pfd.fd);
						auto incr = it->second.getIncrement(pfd.fd, channel.getMax() / 20.);
						wasIncreasing = incr > 0;
						auto old = channel.getValue();
						if (old == 0) {
							if (&channel == &controller.value && incr < 0) {
								channel.set(1);
							} else {
								channel.set(channel.getMax() / 2048);
							}
						} else {
							channel.set(old * (1.0 + incr));
						}
							
								std::cerr << "set " << channel.getName() << " to " << channel.getValue() << " from " << old << " dt is " << std::chrono::duration_cast<std::chrono::duration<float>>(dt).count() << "\n";
					}
					pfd.revents = 0;
				}
			}
		} else {
			if (controller.autoHue) {
				auto now = std::chrono::system_clock::now();
				if (now - lastAutoHueSet > std::chrono::minutes(5)) {
					auto nowAsTime_t = std::chrono::system_clock::to_time_t(now);
					auto lt = localtime(&nowAsTime_t);
					float hour = lt->tm_hour + lt->tm_min / 60.0 + lt->tm_sec / 3600.0;
					if (hour > 19.0) {
						auto phase = (hour - 19.0) / (24. - 19.);
						controller.hue.set(60 - 60*phase);
						controller.saturation.set(phase);
					} else if (hour < 6.0) {
						controller.hue.set(0);
						controller.saturation.set(1);
					} else if (hour < 8.0) {
						auto phase = (hour - 6.0) / (8.0 - 6.0);
						controller.hue.set(phase * 60);
						controller.saturation.set(1 - phase);
					} else {
						controller.saturation.set(0);
					}
					lastAutoHueSet = now;
				}
			}
		}

	}

	daemon->fWaitForThreads();
}
