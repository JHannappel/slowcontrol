#include "slowcontrolDaemon.h"
#include "measurement.h"
#include "slowcontrol.h"
#include <Options.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

class heartBeatSkew: public boundCheckerInterface<SlowcontrolMeasurement<float>>,
	        public unitInterface {
  public:
	heartBeatSkew(const std::string& aName):
		boundCheckerInterface(std::chrono::minutes(10), std::chrono::minutes(1),
		                      0.01, -0.1, 0.1),
		unitInterface(lConfigValues, "s") {
		std::string description(aName);
		description += " heart beat skew";
		fInitializeUid(description);
		fConfigure();
	};
};

slowcontrolDaemon* slowcontrolDaemon::gInstance = nullptr;
Option<bool> gDontDaemonize('\0', "nodaemon", "switch of going to daemon mode", false);

Option<const char*> gPidFileName('\0', "pidFile", "name of pid file", nullptr);

slowcontrolDaemon::slowcontrolDaemon(const char *aName) {
	gInstance = this;
	if (!gDontDaemonize) {
		fDaemonize();
	}
	if (gPidFileName.fGetValue() != nullptr) {
		std::ofstream pidfile(gPidFileName.fGetValue());
		pidfile << getpid() << std::endl;
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
	sigset_t sigmask;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGUSR1);
	sigaddset(&sigmask, SIGUSR2);
	pthread_sigmask(SIG_BLOCK, &sigmask, nullptr);
	lStopRequested = false;
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
	auto writer = dynamic_cast<writeValueInterface*>(aMeasurement);
	if (writer != nullptr) {
		writeableMeasurement wM(aMeasurement, writer);
		lWriteableMeasurements.emplace(aMeasurement->fGetUid(), wM);
	}
}
slowcontrolDaemon* slowcontrolDaemon::fGetInstance() {
	return gInstance;
}


std::chrono::system_clock::time_point slowcontrolDaemon::fBeatHeart(bool aLastTime) {
	std::string query("UPDATE daemon_heartbeat SET daemon_time=(SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ");
	auto now = std::chrono::system_clock::now();
	query += std::to_string(std::chrono::duration<double, std::nano>(now.time_since_epoch()).count() / 1E9);
	query += "* INTERVAL '1 second'), server_time=now(), next_beat=";
	auto nextTime = now + lHeartBeatFrequency;
	if (aLastTime) {
		query += "'infinity'";
	} else {
		query += "(SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
		query += std::to_string(std::chrono::duration<double, std::nano>(nextTime.time_since_epoch()).count() / 1E9);
		query += "* INTERVAL '1 second')";
	}
	query += " where daemonid=";
	query += std::to_string(lId);
	query += " RETURNING extract('epoch' from daemon_time - server_time);";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	auto skew = std::stof(PQgetvalue(result, 0, 0));
	lHeartBeatSkew->fStore(skew, now);
	PQclear(result);
	return nextTime;
}

void slowcontrolDaemon::fSignalCatcherThread() {
	sigset_t sigmask;
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIGTERM);
	sigaddset(&sigmask, SIGINT);
	sigaddset(&sigmask, SIGUSR1);
	sigaddset(&sigmask, SIGUSR2);
	pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr);
	struct sigaction action;
	action.sa_handler = SIG_IGN;
	sigaction(SIGTERM, &action, nullptr);
	sigaction(SIGINT, &action, nullptr);
	sigaction(SIGUSR1, &action, nullptr);
	sigaction(SIGUSR2, &action, nullptr);
	std::cerr << "unblocked signal " <<  std::endl;
	while (true) {
		int sig;
		std::cerr << "wait for signal " <<  std::endl;
		sigwait(&sigmask, &sig);
		std::cerr << "caught signal " << sig << std::endl;
		switch (sig) {
			case SIGTERM:
			case SIGINT:
				fGetInstance()->lStopRequested = true;
				fGetInstance()->lWaitCondition.notify_all();
				std::cerr << "stopping signal thread" << std::endl;
				return;
			default     :
				break;
		}
	}
}

void slowcontrolDaemon::fReaderThread() {
	auto nextHeartBeatTime = fGetInstance()->fBeatHeart();
	while (true) {
		if (fGetInstance()->lMeasurementsWithDefaultReader.empty()) {
			if (fGetInstance()->lStopRequested) {
				fGetInstance()->fBeatHeart(true);
				std::cerr << "stopping reader thread" << std::endl;
				return;
			}
			if (std::chrono::system_clock::now() > nextHeartBeatTime) {
				nextHeartBeatTime = fGetInstance()->fBeatHeart();
			}

			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
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
		while (fGetInstance()->lMeasurementsWithDefaultReader.size()
		        == scheduledMeasurements.size()) {
			if (fGetInstance()->lStopRequested) {
				fGetInstance()->fBeatHeart(true);
				std::cerr << "stopping reader thread" << std::endl;
				return;
			}

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
		bool quitNow = fGetInstance()->lStopRequested;
		for (auto p : fGetInstance()->lMeasurements) {
			auto measurement = p.second;
			measurement->fSendValues();
		}
		if (quitNow) {
			std::cerr << "stopping storer thread" << std::endl;
			return;
		}
		std::unique_lock<decltype(lStorerMutex)> lock(fGetInstance()->lStorerMutex);
		fGetInstance()->lStorerCondition.wait(lock);
	}
}

void slowcontrolDaemon::fSignalToStorer() {
	lStorerCondition.notify_all();
}
void slowcontrolDaemon::fProcessPendingRequests() {
	std::string query("SELECT uid,request,id FROM setvalue_requests WHERE response_time IS NULL AND uid IN (SELECT uid FROM uid_daemon_connection WHERE daemonid = ");
	query += std::to_string(lId);
	query += ") ORDER BY request_time;";
	auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
	for (int i = 0; i < PQntuples(result); i++) {
		auto uid = std::stol(PQgetvalue(result, i, PQfnumber(result, "uid")));
		std::string request(PQgetvalue(result, i, PQfnumber(result, "request")));
		auto id = std::stol(PQgetvalue(result, i, PQfnumber(result, "id")));
		std::string response;
		auto it = lWriteableMeasurements.find(uid);
		if (it == lWriteableMeasurements.end()) {
			response = "is not a wrietable value";
		} else {
			response = it->second.lWriter->fProcessRequest(request);
		}
		query = "UPDATE setvalue_requests SET response_time=now(), response=";
		slowcontrol::fAddEscapedStringToQuery(response, query);
		query += "WHERE id = ";
		query += std::to_string(id);
		query += ";";
		auto subResult = PQexec(slowcontrol::fGetDbconn(), query.c_str());
		PQclear(subResult);
	}
	PQclear(result);
}

void slowcontrolDaemon::fConfigChangeListener() {
	auto res = PQexec(slowcontrol::fGetDbconn(), "LISTEN uid_configs_update");
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		std::cerr << "LISTEN command failed" <<  PQerrorMessage(slowcontrol::fGetDbconn()) << std::endl;
		PQclear(res);
		return;
	}
	PQclear(res);
	res = PQexec(slowcontrol::fGetDbconn(), "LISTEN setvalue_request");
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
		if (poll(&pfd, 1, 100) == 0) {
			if (fGetInstance()->lStopRequested) {
				std::cerr << "stopping cfg change listener thread" << std::endl;
				return;
			}
		}

		bool gotNotificaton = false;

		if (pfd.revents & (POLLIN | POLLPRI)) {
			PQconsumeInput(slowcontrol::fGetDbconn());
			while (true) {
				auto notification = PQnotifies(slowcontrol::fGetDbconn());
				if (notification == nullptr) {
					break;
				}
				std::cout << "got notification '" << notification->relname << "'" << std::endl;
				if (strcmp(notification->relname, "uid_configs_update") == 0) {
					gotNotificaton = true;
				} else if (strcmp(notification->relname, "setvalue_request") == 0) {
					fGetInstance()->fProcessPendingRequests();
				}
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
	lSignalCatcherThread = new std::thread(fSignalCatcherThread);
	lReaderThread = new std::thread(fReaderThread);
	lStorerThread = new std::thread(fStorerThread);
	lConfigChangeListenerThread = new std::thread(fConfigChangeListener);
}
void slowcontrolDaemon::fWaitForThreads() {
	lReaderThread->join();
	lStorerThread->join();
	lConfigChangeListenerThread->join();
	lSignalCatcherThread->join();
}


