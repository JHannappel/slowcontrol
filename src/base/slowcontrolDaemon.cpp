#include "slowcontrolDaemon.h"
#include "measurement.h"
#include "slowcontrol.h"
#include <Options.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

slowcontrolDaemon* slowcontrolDaemon::gInstance = nullptr;
Option<bool> gDontDaemonize('\0', "nodaemon", "switch of going to daemon mode", false);
slowcontrolDaemon::slowcontrolDaemon() {
	gInstance = this;
	if (!gDontDaemonize) {
		fDaemonize();
	}
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
	auto reader = dynamic_cast<defaultReaderInterface*>(aMeasurement);
	if (reader != nullptr) {
		lMeasurementsWithDefaultReader.emplace_back(aMeasurement, reader);
	}
}
slowcontrolDaemon* slowcontrolDaemon::fGetInstance() {
	return gInstance;
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


