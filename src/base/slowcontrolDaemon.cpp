#include "slowcontrolDaemon.h"
#include "measurement.h"
#include "slowcontrol.h"
#include <Options.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class heartBeatSkew: public boundCheckerInterface<SlowcontrolMeasurementFloat> {
  public:
	heartBeatSkew(const std::string& aName):
		boundCheckerInterface(std::chrono::minutes(10), std::chrono::minutes(1),
		                      0.01, -0.1, 0.1) {
		std::string description(aName);
		description += " heart beat skew";
		fInitializeUid(description);
		fConfigure();
	};
};

slowcontrolDaemon* slowcontrolDaemon::gInstance = nullptr;
Option<bool> gDontDaemonize('\0', "nodaemon", "switch of going to daemon mode", false);
slowcontrolDaemon::slowcontrolDaemon(const char *aName) {
	gInstance = this;
	if (!gDontDaemonize) {
		fDaemonize();
	}
	std::string description(aName);
	description += " on ";
	description += slowcontrol::fGetHostName();
	lId = slowcontrol::fSelectOrInsert("daemon_list", "daemonid",
	                                   "description", description.c_str());
	std::string query("DELETE FROM uid_daemon_connection WHERE daemonid = ");
	query += std::to_string(lId);
	query += ";";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	PQclear(result);
	query = "INSERT INTO daemon_heartbeat(daemonid) VALUES (";
	query += std::to_string(lId);
	query += ");";
	result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	PQclear(result);
	lHeartBeatFrequency = std::chrono::minutes(1);
	lHeartBeatSkew = new heartBeatSkew(description);
	slowcontrol::fAddToCompound(slowcontrol::fGetCompoundId("generalSlowcontrol", "slowcontrol internal general stuff"),
	                            lHeartBeatSkew->fGetUid(), "description");
};

void slowcontrolDaemon::fDaemonize() {
	auto daemon_pid = fork();
	if (daemon_pid == 0) { /* now we are the forked process */
		int fd;
		fd = open("/dev/null", O_RDONLY);
		dup2(fd, 0);
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, 1);
		fd = open("/dev/null", O_WRONLY);
		dup2(fd, 2);
		setsid();       // detach from old session;
		return;
	} else if (daemon_pid != -1) { /* now we are still the old process */
		exit(0);
	} else { /* dirt! the fork failed! */
		std::cerr << "could not fork daemon" << std::endl;
		exit(1);
	}
}

void slowcontrolDaemon::fRegisterMeasurement(SlowcontrolMeasurementBase* aMeasurement) {
	lMeasurements.emplace(aMeasurement->fGetUid(), aMeasurement);
	std::string query("INSERT INTO uid_daemon_connection (uid, daemonid) VALUES (");
	query += std::to_string(aMeasurement->fGetUid());
	query += ",";
	query += std::to_string(lId);
	query += ");";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	PQclear(result);
	auto reader = dynamic_cast<defaultReaderInterface*>(aMeasurement);
	if (reader != nullptr) {
		lMeasurementsWithDefaultReader.emplace_back(aMeasurement, reader);
	}
}
slowcontrolDaemon* slowcontrolDaemon::fGetInstance() {
	return gInstance;
}


std::chrono::system_clock::time_point slowcontrolDaemon::fBeatHeart() {
	std::string query("UPDATE daemon_heartbeat SET daemon_time=(SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ");
	auto now = std::chrono::system_clock::now();
	query += std::to_string(std::chrono::duration<double, std::nano>(now.time_since_epoch()).count() / 1E9);
	query += "* INTERVAL '1 second'), server_time=now(), next_beat=(SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
	auto nextTime = now + lHeartBeatFrequency;
	query += std::to_string(std::chrono::duration<double, std::nano>(nextTime.time_since_epoch()).count() / 1E9);
	query += "* INTERVAL '1 second') where daemonid=";
	query += std::to_string(lId);
	query += " RETURNING extract('epoch' from daemon_time - server_time);";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	auto skew = std::stof(PQgetvalue(result, 0, 0));
	lHeartBeatSkew->fStore(skew, now);
	PQclear(result);
	return nextTime;
}


void slowcontrolDaemon::fReaderThread() {
	while (true) {
		std::chrono::system_clock::duration maxReadoutInterval(0);
		for (auto& measurement : fGetInstance()->lMeasurementsWithDefaultReader) {
			if (maxReadoutInterval < measurement.lBase->fGetReadoutInterval()) {
				maxReadoutInterval = measurement.lBase->fGetReadoutInterval();
			}
		}
		maxReadoutInterval /= fGetInstance()->lMeasurementsWithDefaultReader.size();
		std::multimap<std::chrono::system_clock::time_point, defaultReadableMeasurement> scheduledMeasurements;
		auto then = std::chrono::system_clock::now();
		for (auto& measurement : fGetInstance()->lMeasurementsWithDefaultReader) {
			std::cout << "scheduled uid " << measurement.lBase->fGetUid() << " for " <<
			          std::chrono::duration_cast<std::chrono::seconds>(then.time_since_epoch()).count() << "\n";
			scheduledMeasurements.emplace(then, measurement);
			then += maxReadoutInterval;

		}
		auto nextHeartBeatTime = fGetInstance()->fBeatHeart();
		while (fGetInstance()->lMeasurementsWithDefaultReader.size()
		        == scheduledMeasurements.size()) {
			auto it = scheduledMeasurements.begin();
			std::this_thread::sleep_until(it->first);
			auto measurement = it->second;

			auto justBeforeReadout = std::chrono::system_clock::now();
			measurement.lReader->fReadCurrentValue();

			scheduledMeasurements.erase(it);
			scheduledMeasurements.emplace(justBeforeReadout
			                              + measurement.lBase->fGetReadoutInterval(),
			                              measurement);
			if (justBeforeReadout > nextHeartBeatTime) {
				nextHeartBeatTime = fGetInstance()->fBeatHeart();
			}
		}
	}
}
void slowcontrolDaemon::fStorerThread() {
	while (true) {
		for (auto p : fGetInstance()->lMeasurements) {
			auto measurement = p.second;
			measurement->fSendValues();
		}
		std::unique_lock<decltype(lStorerMutex)> lock(fGetInstance()->lStorerMutex);
		fGetInstance()->lStorerCondition.wait(lock);
	}
}

void slowcontrolDaemon::fSignalToStorer() {
	lStorerCondition.notify_all();
}

void slowcontrolDaemon::fConfigChangeListener() {
	auto res = PQexec(slowcontrol::fGetDbconn(), "LISTEN uid_configs_update");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		std::cerr << "LISTEN command failed" <<  PQerrorMessage(slowcontrol::fGetDbconn()) << std::endl;
		PQclear(res);
		return;
	}
	PQclear(res);
	while (true) {
		struct pollfd pfd;
		pfd.fd = PQsocket(slowcontrol::fGetDbconn());
		pfd.events = POLLIN | POLLPRI;
		poll(&pfd, 1, -1);

		bool gotNotificaton = false;

		if (pfd.revents & (POLLIN | POLLPRI)) {
			PQconsumeInput(slowcontrol::fGetDbconn());
			while (true) {
				auto notification = PQnotifies(slowcontrol::fGetDbconn());
				if (notification == nullptr) {
					break;
				}
				gotNotificaton = true;
				PQfreemem(notification);
			}
		}
		if (gotNotificaton) {
			for (auto p : fGetInstance()->lMeasurements) {
				auto measurement = p.second;
				measurement->fConfigure();
			}
		}

	}
}

void slowcontrolDaemon::fStartThreads() {
	lReaderThread = new std::thread(fReaderThread);
	lStorerThread = new std::thread(fStorerThread);
	lConfigChangeListenerThread = new std::thread(fConfigChangeListener);
}
void slowcontrolDaemon::fWaitForThreads() {
	lReaderThread->join();
	lStorerThread->join();
	lConfigChangeListenerThread->join();
}


