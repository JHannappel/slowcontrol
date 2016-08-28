#include "measurement.h"
#include "slowcontrolDaemon.h"
#include "slowcontrol.h"
#include <fstream>
#include <Options.h>

#include <dirent.h>
#include <string.h>
#include <iostream>
#include <limits>
#include <sys/vfs.h>

class cpuTemperature: public boundCheckerInterface<SlowcontrolMeasurementFloat>,
	public defaultReaderInterface

{
  protected:
	std::string lPath;
  public:
	cpuTemperature(const char *aPath):
		boundCheckerInterface(std::chrono::minutes(5),
		                      std::chrono::seconds(30),
		                      2, 10, 90),
		lPath(aPath) {
		std::string name;
		name = slowcontrol::fGetHostName();
		name += ":";
		name += aPath;
		lDeadBand.fSetValue(1);
		fInitializeUid(name);
		fConfigure();
	};
	virtual void fReadCurrentValue() {
		std::ifstream thermometer(lPath.c_str());
		float temperature;
		thermometer >> temperature;
		fStore(temperature * 0.001);
	};
};

class diskValue: public SlowcontrolMeasurementFloat {
  public:
	diskValue(const std::string& aSerial,
	          int aId,
	          const char *aName,
	          int aDiskCompound) :
		SlowcontrolMeasurementFloat(std::chrono::minutes(60),
		                            std::chrono::minutes(10),
		                            1) {
		std::string description("disk");
		description += aSerial;
		description += "_";
		description += std::to_string(aId);
		description += "_";
		description += aName;
		fInitializeUid(description);
		fConfigure();
		slowcontrol::fAddToCompound(aDiskCompound, fGetUid(), aName);
	};
};

class freeMemory: public SlowcontrolMeasurementFloat,
	public defaultReaderInterface {
  public:
	freeMemory(int aHostCompound):
		SlowcontrolMeasurementFloat(std::chrono::minutes(30),
		                            std::chrono::seconds(10),
		                            1000) {
		std::string description;
		description = slowcontrol::fGetHostName();
		description += ":free_memory";
		fInitializeUid(description);
		fConfigure();
		slowcontrol::fAddToCompound(aHostCompound, fGetUid(), "freeMemory");
	}
	virtual void fReadCurrentValue() {
		FILE *f;
		double v = 0;
		f = fopen("/proc/meminfo", "r");
		if (fscanf(f, "MemTotal: %lf kB\n", &v) == 1) {
			if (fscanf(f, "MemFree: %lf kB\n", &v) == 1) {
				//FIXME
				// Newer meminfo contain an additional line "MemAvailable" here.
				if (fscanf(f, "MemAvailable: %lf kB\n", &v) == 1) {
					fStore(v);
				}
			}
		}
		fclose(f);
	};
};


class fsSize: public SlowcontrolMeasurementFloat,
	public defaultReaderInterface {
  protected:
	std::string lMountPoint;
  public:
	fsSize(int aHostCompound,
	       const std::string& aDevice,
	       const std::string& aMountPoint):
		SlowcontrolMeasurementFloat(std::chrono::minutes(20),
		                            std::chrono::seconds(10),
		                            1),
		lMountPoint(aMountPoint) {
		std::string description("free space on ");
		description += slowcontrol::fGetHostName();
		description += " ";
		description += aDevice;
		lDeadBand.fSetValue(10);
		fInitializeUid(description);
		fConfigure();
		description = aMountPoint;
		description += "free space";
		slowcontrol::fAddToCompound(aHostCompound, fGetUid(), description);
	};
	virtual void fReadCurrentValue() {
		struct statfs buf;
		if (statfs(lMountPoint.c_str(), &buf) == 0) {
			fStore((double)buf.f_bavail * (double)buf.f_bsize / (1024. * 1024. * 1024.));
		};
	};
};

class diskwatch {
  private:
	std::string lCommand;
	std::string lFamily;
	std::string lModel;
	std::string lSerial;
	std::map <int, SlowcontrolMeasurementFloat*>values;
	int id;
	int lHostCompound;
  public:
	diskwatch(const char *aPath, int aHostCompound);
	void read();
};
diskwatch::diskwatch(const char *aPath, int aHostCompound) {
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
		if (fgets(line, sizeof(line), p) == NULL) {
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
		id = slowcontrol::fGetCompoundId(compoundName.c_str(), compoundName.c_str());
		slowcontrol::fAddSubCompound(lHostCompound, id, "disk");
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
		slowcontrol::fAddToCompound(aCompound, t->fGetUid(), "temperature");
	}
	pclose(findPipe);
}

static void populateDiskwatches(int aCompund, std::vector<diskwatch*>& aDiskwatches) {
	DIR *devdir = opendir("/dev");
	for (;;) {
		struct dirent *de = readdir(devdir);
		if (de == NULL) {
			break;
		}
		if (strncmp("sd", de->d_name, 2) == 0 && strlen(de->d_name) == 3) {
			char name[256] = "/dev/";
			strcat(name, de->d_name);
			char testcmd[256] = "smartctl -s on ";
			strcat(testcmd, name);
			if (system(testcmd) == 0) {
				diskwatch *dw = new diskwatch(name, aCompund);
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
	OptionParser parser("slowcontrol program for checking comuter health");
	parser.fParse(argc, argv);
	auto daemon = new slowcontrolDaemon;
	auto compound = slowcontrol::fGetCompoundId(slowcontrol::fGetHostName().c_str(), slowcontrol::fGetHostName().c_str());
	std::vector<diskwatch*> diskwatches;
	populateTemperature(compound);
	populateDiskwatches(compound, diskwatches);
	populateFswatches(compound);
	new freeMemory(compound);
	daemon->fStartThreads();
	while (true) {
		for (auto dw : diskwatches) {
			dw->read();
			sleep(600);
		}
	}
	daemon->fWaitForThreads();
}
