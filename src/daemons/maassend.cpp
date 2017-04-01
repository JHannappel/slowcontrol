#include "measurement.h"
#include "slowcontrol.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>

#include <dirent.h>
#include <iostream>
#include <limits>
#include <string.h>
#include <sys/vfs.h>
using namespace slowcontrol;
class cpuTemperature: public boundCheckerInterface<measurement<float>, false, true>,
	public defaultReaderInterface {
  protected:
	std::string lPath;
  public:
	cpuTemperature(const char *aPath):
		boundCheckerInterface(10, 90, 2),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(30)),
		lPath(aPath) {
		lClassName.fSetFromString(__func__);
		std::string name;
		name = slowcontrol::base::fGetHostName();
		name += ":";
		name += aPath;
		lDeadBand.fSetValue(1);
		fInitializeUid(name);
		fConfigure();
		std::cerr << __func__ << " size is " << sizeof(*this) << std::endl;
	};
	bool fReadCurrentValue() override {
		std::ifstream thermometer(lPath.c_str());
		float temperature;
		thermometer >> temperature;
		return fStore(temperature * 0.001);
	};
};

class diskValue: public boundCheckerInterface<measurement<float>> {
  public:
	diskValue(const std::string& aSerial,
	          int aId,
	          const char *aName,
	          int aDiskCompound) :
		boundCheckerInterface(std::numeric_limits<valueType>::lowest(), std::numeric_limits<valueType>::max(), 1) {
		lClassName.fSetFromString(__func__);
		std::string description("disk");
		description += aSerial;
		description += "_";
		description += std::to_string(aId);
		description += "_";
		description += aName;
		fInitializeUid(description);
		fConfigure();
		description = "smartvalue ";
		description += std::to_string(aId);
		auto svCompound = base::fGetCompoundId(description.c_str(), aName);
		auto svsCompound = base::fGetCompoundId("smartvalues", "all smartvalues");
		base::fAddSubCompound(svsCompound, svCompound, aName);
		base::fAddToCompound(svCompound, fGetUid(), aSerial);
		base::fAddToCompound(aDiskCompound, fGetUid(), aName);
	};
};

class freeMemory: public boundCheckerInterface<measurement<float>, true, false>,
	public defaultReaderInterface {
  public:
	freeMemory(int aHostCompound):
		boundCheckerInterface(2000, 0, 1000),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(10)) {
		lClassName.fSetFromString(__func__);
		std::string description;
		description = slowcontrol::base::fGetHostName();
		description += ":free_memory";
		fInitializeUid(description);
		fConfigure();
		slowcontrol::base::fAddToCompound(aHostCompound, fGetUid(), "freeMemory");
		std::cerr << __func__ << " size is " << sizeof(*this) << std::endl;
	}
	bool fReadCurrentValue() override {
		bool valueHasChanged = false;
		FILE *f;
		double v = 0;
		f = fopen("/proc/meminfo", "r");
		if (fscanf(f, "MemTotal: %lf kB\n", &v) == 1) {
			if (fscanf(f, "MemFree: %lf kB\n", &v) == 1) {
				//FIXME
				// Newer meminfo contain an additional line "MemAvailable" here.
				if (fscanf(f, "MemAvailable: %lf kB\n", &v) == 1) {
					valueHasChanged = fStore(v);
				}
			}
		}
		fclose(f);
		return valueHasChanged;
	};
};


class fsSize: public boundCheckerInterface<measurement<float>, true, false>,
	public defaultReaderInterface {
  protected:
	configValue<std::string> lName;
	std::string lMountPoint;
  public:
	fsSize(int aHostCompound,
	       const std::string& aDevice,
	       const std::string& aMountPoint):
		boundCheckerInterface(10, 0, 0.001),
		defaultReaderInterface(lConfigValues, std::chrono::seconds(10)),
		lName("name", lConfigValues),
		lMountPoint(aMountPoint) {
		lClassName.fSetFromString(__func__);
		std::string description("free space on ");
		description += slowcontrol::base::fGetHostName();
		description += " ";
		description += aDevice;
		fInitializeUid(description);
		description = aMountPoint;
		description += " free space";
		lName.fSetValue(description);
		fConfigure();
		slowcontrol::base::fAddToCompound(aHostCompound, fGetUid(), description);
		std::cerr << __func__ << " size is " << sizeof(*this) << std::endl;
	};
	bool fReadCurrentValue() override {
		struct statfs buf;
		if (statfs(lMountPoint.c_str(), &buf) == 0) {
			return fStore(static_cast<double>(buf.f_bavail) * static_cast<double>(buf.f_bsize) / (1024. * 1024. * 1024.));
		};
		return false;
	};
};

class diskwatch {
  private:
	std::string lCommand;
	std::string lFamily;
	std::string lModel;
	std::string lSerial;
	std::map <int, diskValue*>values;
	int id;
	int lHostCompound;
  public:
	diskwatch(const std::string& aPath, int aHostCompound);
	void read();
};
diskwatch::diskwatch(const std::string& aPath, int aHostCompound) {
	lCommand = "smartctl -s on -i -A ";
	lCommand += aPath;
	id = 0;
	lHostCompound = aHostCompound;
}
void diskwatch::read() {
	auto p = popen(lCommand.c_str(), "r");
	bool drive_changed = false;
	while (!feof(p)) {
		char line[1024];
		char buf[1024];
		if (fgets(line, sizeof(line), p) == nullptr) {
			break;
		}
		if (sscanf(line, "Model Family: %[^\n\r]", buf) == 1) { // found drive model family
			if (lFamily.compare(buf) != 0) {
				drive_changed = true;
				lFamily = buf;
			}
		} else if (sscanf(line, "Device Model: %[^\n\r]", buf) == 1) { // found drive model
			if (lModel.compare(buf) != 0) {
				drive_changed = true;
				lModel = buf;
			}
		} else if (sscanf(line, "Serial Number: %[^\n\r]", buf) == 1) { // found serial
			if (strcmp(buf, "[No Information Found]") != 0) { // we really have a serial number
				if (lSerial.compare(buf) != 0) {
					drive_changed = true;
					lSerial = buf;
				}
			}
		} else if (id > 0) { // only if we know the disk look for individual values
			int ID, flag, value, worst, thresh;
			double raw;
			char buf2[1024];
			int items;
			bool found = false;
			items = sscanf(line, "%d %s %x %d %d --- %*s %*s %s %lf",
			               &ID, buf2, &flag, &value, &worst, buf, &raw);
			if (items == 7) { // we could not get thresholds, so assume fine
				thresh = 0;
				found = true;
			} else {
				items = sscanf(line, "%d %s %x %d %d %d %*s %*s %s %lf",
				               &ID, buf2, &flag, &value, &worst, &thresh, buf, &raw);
				if (items == 8) {
					found = true;
				}
			}
			if (found) {// attribute read sucessfully
				auto it = values.find(ID);
				if (it == values.end()) {
					auto bla = values.emplace(ID, new diskValue(lSerial, ID, buf2, id));
					it = bla.first;
				}
				it->second->fStore(raw);
			}
		}
	}
	pclose(p);
	if (drive_changed) {
		std::string compoundName("disk_");
		compoundName += lFamily;
		compoundName += "_";
		compoundName += lModel;
		compoundName += "_";
		compoundName += lSerial;
		id = slowcontrol::base::fGetCompoundId(compoundName.c_str(), compoundName.c_str());
		slowcontrol::base::fAddSubCompound(lHostCompound, id, "disk");
	}
}

static void populateTemperature(int aCompound) {
	if (system("modprobe coretemp") != 0) {
		std::cerr << "modprobe coretemp failed, there may be problems" << std::endl;
	}
	FILE *findPipe = popen("find /sys/devices/platform/coretemp.* -name 'temp*_input' 2>/dev/null;find -L /sys/class/thermal/ -maxdepth 2 -name temp 2>/dev/null", "r");
	for (;;) {
		char dirpath[1024];
		if (feof(findPipe) || ferror(findPipe)) {
			break;
		}
		if (fgets(dirpath, sizeof(dirpath), findPipe) == nullptr) {
			break;
		}
		auto *newLine = strchr(dirpath, '\n');
		if (newLine) {
			*newLine = '\0';
		}
		auto t = new cpuTemperature(dirpath);
		slowcontrol::base::fAddToCompound(aCompound, t->fGetUid(), "temperature");
	}
	pclose(findPipe);
}

static void populateDiskwatches(int aCompund, std::vector<diskwatch*>& aDiskwatches) {
	DIR *devdir = opendir("/dev");
	for (;;) {
		auto de = readdir(devdir);
		if (de == nullptr) {
			break;
		}
		if (strncmp("sd", de->d_name, 2) == 0 && strlen(de->d_name) == 3) {
			std::string name("/dev/");
			name += de->d_name;
			std::string testcmd("smartctl -s on ");
			testcmd += name;
			if (system(testcmd.c_str()) == 0) {
				auto dw = new diskwatch(name, aCompund);
				dw->read();
				aDiskwatches.push_back(dw);
				std::cerr << name << " found as hard disk" << std::endl;
			} else {
				std::cerr << name << " seems to be no proper disk" << std::endl;
			}
		}
	}
	closedir(devdir);
}

static void populateFswatches(int aHostCompound) {
	std::ifstream mtab("/proc/mounts");
	std::string device;
	std::string mountpoint;
	std::string fstype;
	std::cerr << "start read mtab \n";
	while (!mtab.eof() && !mtab.bad()) {
		mtab >> device;
		mtab >> mountpoint;
		mtab >> fstype;
		mtab.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cerr << "maybe found " << device << " mounted on " << mountpoint << " as " << fstype << std::endl;
		if (fstype.compare("ext3") == 0
		        || fstype.compare("ext4") == 0
		        || fstype.compare("btrfs") == 0) {
			std::cerr << "found " << device << " mounted on " << mountpoint << std::endl;
			new fsSize(aHostCompound, device, mountpoint);
		}
	}
	std::cerr << "stop read mtab \n";
}

int main(int argc, const char *argv[]) {
	options::parser parser("slowcontrol program for checking comuter health");
	parser.fParse(argc, argv);
	auto maassend = new slowcontrol::daemon("maassend");
	auto compound = base::fGetCompoundId(base::fGetHostName().c_str(), base::fGetHostName().c_str());
	std::vector<diskwatch*> diskwatches;
	populateTemperature(compound);
	populateDiskwatches(compound, diskwatches);
	populateFswatches(compound);
	new freeMemory(compound);
	maassend->fStartThreads();
	if (!diskwatches.empty()) {
		while (!maassend->fGetStopRequested()) {
			for (auto dw : diskwatches) {
				dw->read();
				std::cerr << "waiting..." << maassend->fGetStopRequested() << std::endl;
				maassend->fWaitFor(std::chrono::seconds(600 / diskwatches.size()));
				std::cerr << "done." << maassend->fGetStopRequested() << std::endl;
				if (maassend->fGetStopRequested()) {
					break;
				}
			}
		}
	}
	std::cerr << "stopping main thread" << std::endl;
	maassend->fWaitForThreads();
}
