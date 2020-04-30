#include "slowcontrolDaemon.h"
#include "measurement.h"
#include "slowcontrol.h"
#include <Options.h>
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include "pgsqlWrapper.h"

namespace slowcontrol {
	static std::map<std::chrono::system_clock::time_point, writeValue::request*> gScheduledWriteRequests;

	class heartBeatSkew: public boundCheckerInterface<measurement<float>>,
		        public unitInterface {
	  public:
		heartBeatSkew(const std::string& aName):
			boundCheckerInterface(-0.1, 0.1, 0.01),
			unitInterface(lConfigValues, "s") {
			lClassName.fSetFromString(__func__);
			std::string description(aName);
			description += " heart beat skew";
			fInitializeUid(description);
			fConfigure();
		};
	};

	daemon* daemon::gInstance = nullptr;
	options::single<bool> gDontDaemonize('\0', "nodaemon", "switch of going to daemon mode", false);

	options::single<const char*> gPidFileName('\0', "pidFile", "name of pid file", nullptr);

	daemon::daemon(const char *aName) :
		lThreads([](std::thread * a, std::thread * b) {
		return a->get_id() < b->get_id();
	}) {
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
		description += base::fGetHostName();
		lId = base::fSelectOrInsert("daemon_list", "daemonid",
		                            "description", description.c_str());
		std::string query("DELETE FROM uid_daemon_connection WHERE daemonid = ");
		query += std::to_string(lId);
		query += ";";
		pgsql::request{query};
		query = "INSERT INTO daemon_heartbeat(daemonid) VALUES (";
		query += std::to_string(lId);
		query += ");";
		pgsql::request{query};
		lHeartBeatPeriod = std::chrono::minutes(1);
		lHeartBeatSkew = new heartBeatSkew(description);
		base::fAddToCompound(base::fGetCompoundId("generalSlowcontrol", "slowcontrol internal general stuff"),
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

	void daemon::fDaemonize() {
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
		}
		if (daemon_pid != -1) { /* now we are still the old process */
			exit(0);
		} else { /* dirt! the fork failed! */
			std::cerr << "could not fork daemon" << std::endl;
			exit(1);
		}
	}

	void daemon::fRegisterMeasurement(measurementBase* aMeasurement) {
		{
			std::lock_guard<decltype(lMeasurementsMutex)> lock(lMeasurementsMutex);
			lMeasurements.emplace(aMeasurement->fGetUid(), aMeasurement);
		}
		std::string query("INSERT INTO uid_daemon_connection (uid, daemonid) VALUES (");
		query += std::to_string(aMeasurement->fGetUid());
		query += ",";
		query += std::to_string(lId);
		query += ");";
		pgsql::request{query};

		auto reader = dynamic_cast<defaultReaderInterface*>(aMeasurement);
		if (reader != nullptr) {
			std::lock_guard<decltype(lMeasurementsWithReaderMutex)> lock(lMeasurementsWithReaderMutex);
			lMeasurementsWithDefaultReader.emplace_back(aMeasurement, reader);
		}
		auto pollReader = dynamic_cast<pollReaderInterface*>(aMeasurement);
		if (pollReader != nullptr) {
			std::lock_guard<decltype(lMeasurementsWithPollReaderMutex)> lock(lMeasurementsWithPollReaderMutex);
			lMeasurementsWithPollReader.emplace(pollReader->fGetFd(), pollReader);
		}
		auto writer = dynamic_cast<writeValue*>(aMeasurement);
		if (writer != nullptr) {
			writeableMeasurement wM(aMeasurement, writer);
			std::lock_guard<decltype(lMeasurementsWriteableMutex)> lock(lMeasurementsWriteableMutex);

			lWriteableMeasurements.emplace(aMeasurement->fGetUid(), wM);
		}
	}
	daemon* daemon::fGetInstance() {
		return gInstance;
	}


	std::chrono::system_clock::time_point daemon::fBeatHeart(bool aLastTime) {
		std::string query("UPDATE daemon_heartbeat SET daemon_time=(SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ");
		auto now = std::chrono::system_clock::now();
		query += std::to_string(std::chrono::duration<double, std::nano>(now.time_since_epoch()).count() / 1E9);
		query += "* INTERVAL '1 second'), server_time=now(), next_beat=";
		auto nextTime = now + lHeartBeatPeriod;
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
		pgsql::request result(query);
		auto skew = std::stof(result.getValue(0, 0));
		lHeartBeatSkew->fStore(skew, now);
		return nextTime;
	}

	void daemon::fSignalCatcherThread() {
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
		while (true) {
			struct timespec timeout;
			timeout.tv_sec = 1;
			timeout.tv_nsec = 0;
			auto sig = sigtimedwait(&sigmask, nullptr, &timeout);
			switch (sig) {
				case SIGTERM:
				case SIGINT:
					lStopRequested = true;
					lWaitCondition.notify_all();
				default:
					break;
			}
			if (lStopRequested) {
				fFlushAllValues();
				std::cerr << "stopping signal thread" << std::endl;
				return;
			}
		}
	}

	void daemon::fFlushAllValues() {
		for (auto& measurement : lMeasurements) {
			measurement.second->fFlush();
		}
	}

	void daemon::fReaderThread() {
		auto nextHeartBeatTime = fBeatHeart();
		while (true) {
			if (lMeasurementsWithDefaultReader.empty()) {
				if (lStopRequested) {
					fBeatHeart(true);
					std::cerr << "stopping reader thread" << std::endl;
					return;
				}
				if (std::chrono::system_clock::now() > nextHeartBeatTime) {
					nextHeartBeatTime = fBeatHeart();
				}

				fWaitFor(std::chrono::seconds(1));
				continue;
			}
			std::chrono::system_clock::duration maxReadoutInterval(0);
			{
				std::lock_guard < decltype(lMeasurementsWithReaderMutex) > lock(lMeasurementsWithReaderMutex);
				for (auto& measurement : lMeasurementsWithDefaultReader) {
					if (maxReadoutInterval < measurement.lReader->fGetReadoutInterval()) {
						maxReadoutInterval = measurement.lReader->fGetReadoutInterval();
					}
				}
				maxReadoutInterval /= lMeasurementsWithDefaultReader.size();
			}
			if (lHeartBeatPeriod < maxReadoutInterval) {
				lHeartBeatPeriod = maxReadoutInterval;
			}


			std::multimap<std::chrono::steady_clock::time_point, defaultReadableMeasurement> scheduledMeasurements;
			auto then = std::chrono::steady_clock::now();
			{
				std::lock_guard < decltype(lMeasurementsWithReaderMutex) > lock(lMeasurementsWithReaderMutex);
				for (auto& measurement : lMeasurementsWithDefaultReader) {
					std::cout << "scheduled uid " << measurement.lBase->fGetUid() << " for " <<
					          std::chrono::duration_cast<std::chrono::seconds>(then.time_since_epoch()).count() << "\n";
					scheduledMeasurements.emplace(then, measurement);
					then += maxReadoutInterval;
				}
			}
			// here no lock on the mutex is needed for just acessing size()
			while (lMeasurementsWithDefaultReader.size()
			        == scheduledMeasurements.size()) {
				if (lStopRequested) {
					fBeatHeart(true);
					std::cerr << "stopping reader thread" << std::endl;
					return;
				}

				auto it = scheduledMeasurements.begin();
				std::this_thread::sleep_until(it->first);
				auto measurement = it->second;

				auto justBeforeReadout = std::chrono::steady_clock::now();
				if (fExecuteGuarded([measurement]() {
				measurement.lReader->fReadCurrentValue();
				})) {
					continue;
				}
				scheduledMeasurements.erase(it);
				scheduledMeasurements.emplace(justBeforeReadout
				                              + measurement.lReader->fGetReadoutInterval(),
				                              measurement);
				if (std::chrono::system_clock::now() > nextHeartBeatTime) {
					nextHeartBeatTime = fBeatHeart();
				}
			}
		}
	}

	void daemon::fPollerThread() {
		while (true) {
			if (lMeasurementsWithPollReader.empty()) {
				if (lStopRequested) {
					std::cerr << "stopping poller thread" << std::endl;
					return;
				}
				fWaitFor(std::chrono::seconds(1));
				continue;
			}

			std::vector<struct pollfd> pfds;
			{
				std::lock_guard < decltype(lMeasurementsWithPollReaderMutex) > lock(lMeasurementsWithPollReaderMutex);
				for (auto& measurement : lMeasurementsWithPollReader) {
					struct pollfd pfd;
					measurement.second->fSetPollFd(&pfd);
					pfds.emplace_back(pfd);
				}
			}
			// here no lock on the mutex is needed fur just acessing size()
			while (lMeasurementsWithPollReader.size()
			        == pfds.size()) {
				if (lStopRequested) {
					std::cerr << "stopping poller thread" << std::endl;
					return;
				}
				auto result = poll(pfds.data(), pfds.size(), 1000);
				if (result > 0) {
					for (auto& pfd : pfds) {
						if (pfd.revents != 0) {
							auto it = lMeasurementsWithPollReader.find(pfd.fd);
							if (it != lMeasurementsWithPollReader.end()) {
								if (fExecuteGuarded([it, pfd]() {
								it->second->fProcessData(pfd.revents);
								})) {
									continue;
								}
							}
						}
					}
				}
			}
		}
	}
	void daemon::fStorerThread() {
		while (true) {
			bool quitNow = lStopRequested;
			std::vector<measurementBase*> measurements;
			{
				std::lock_guard < decltype(lMeasurementsMutex) > lock(lMeasurementsMutex);
				for (auto& measurement : lMeasurements) {
					if (measurement.second->fValuesToSend()) {
						measurements.emplace_back(measurement.second);
					}
				}
			}
			for (auto measurement : measurements) {
				measurement->fSendValues();
			}
			if (quitNow) {
				std::cerr << "stopping storer thread" << std::endl;
				return;
			}
			std::unique_lock<decltype(lStorerMutex)> lock(lStorerMutex);
			lStorerCondition.wait(lock);
		}
	}

	void daemon::fSignalToStorer() {
		lStorerCondition.notify_all();
	}

	void daemon::fScheduledWriterThread() {
		while (true) {
			if (lStopRequested) {
				std::cerr << "stopping scheduled writer thread" << std::endl;
				return;
			}
			writeValue::request* req;
			{
				std::unique_lock < decltype(lScheduledWriteRequestMutex) > lock(lScheduledWriteRequestMutex);
				auto it = gScheduledWriteRequests.begin();
				if (it != gScheduledWriteRequests.end()) {
					auto waitResult = lScheduledWriteRequestWaitCondition.wait_until(lock, it->first);
					if (waitResult == std::cv_status::no_timeout) {
						continue; // probably a new value added
					}
				} else {
					lScheduledWriteRequestWaitCondition.wait_for(lock, std::chrono::seconds(1));
					continue; // probably a new value added
				}
				if (it->first > std::chrono::system_clock::now()) {
					continue; // it's not yet late enough
				}
				req = it->second;
				gScheduledWriteRequests.erase(it);
			}
			std::string response;
			auto outcome = false;
			fExecuteGuarded([req, &outcome, &response]() {
				outcome = req->fProcess(response);
			});
			std::string query("UPDATE setvalue_requests SET response_time=now(), response=");
			pgsql::fAddEscapedStringToQuery(response, query);
			query += ",result=";
			if (outcome) {
				query += "'true'";
			} else {
				query += "'false'";
			}
			query += " WHERE id=";
			query += std::to_string(req->fGetRequestId());
			pgsql::request{query};
			delete req;
		}
	}

	void daemon::fClearOldPendingRequests() {
		// mark all unprocessed requests for this daemon as missed and failed
		std::string query("UPDATE setvalue_requests SET response='missed', result='false',response_time=now() WHERE response_time IS NULL AND request_time < now() AND uid IN (SELECT uid FROM uid_daemon_connection WHERE daemonid = ");
		query += std::to_string(lId);
		query += ");";
		pgsql::request{query};
	}

	void daemon::fProcessPendingRequests(base::uidType aUid) {
		if (aUid != 0) {
			auto it = lWriteableMeasurements.find(aUid);
			if (it == lWriteableMeasurements.end()) {
				return;
			}
		}
		std::string query("SELECT uid,request,id,EXTRACT('EPOCH' FROM request_time) AS request_time FROM setvalue_requests WHERE response_time IS NULL AND uid IN (SELECT uid FROM uid_daemon_connection WHERE daemonid = ");
		query += std::to_string(lId);
		query += ") ORDER BY request_time;";
		pgsql::request result(query);
		for (int i = 0; i < result.size(); i++) {
			auto uid = std::stol(result.getValue(i, "uid"));
			std::string request(result.getValue(i, "request"));
			auto id = std::stol(result.getValue(i, "id"));
			std::chrono::system_clock::time_point request_time(std::chrono::nanoseconds(static_cast<long long>(std::stod(result.getValue(i, "request_time")) * 1e9)));

			query = "UPDATE setvalue_requests SET response_time=now(), response=";
			auto it = lWriteableMeasurements.find(uid);
			if (it == lWriteableMeasurements.end()) {
				query += "'is not a writeable value', result='false'";
			} else {
				std::string response;
				auto req = it->second.lWriter->fParseForRequest(request, request_time, id);
				if (req == nullptr) {
					query += "'malformed request', result='false'";
				} else if (request_time - std::chrono::system_clock::now() >
				           std::chrono::milliseconds(1)) { // do it delayed
					query += "'scheduled'";
					{
						std::lock_guard<decltype(lScheduledWriteRequestMutex)> lock(lScheduledWriteRequestMutex);
						gScheduledWriteRequests.emplace(request_time, req);
					}
					lScheduledWriteRequestWaitCondition.notify_all();
				} else {
					auto outcome = false;
					fExecuteGuarded([req, &outcome, &response]() {
						outcome = req->fProcess(response);
					});
					delete req;
					pgsql::fAddEscapedStringToQuery(response, query);
					if (outcome == true) {
						query += ",result='true' ";
					} else {
						query += ",result='false' ";
					}
				}
			}
			query += "WHERE id = ";
			query += std::to_string(id);
			query += ";";
			pgsql::request{query};
		}
	}

	void daemon::fConfigChangeListener() {
		pgsql::request("LISTEN uid_configs_update");
		pgsql::request("LISTEN setvalue_request");
		while (true) {
			struct pollfd pfd;
			pfd.fd = pgsql::getFd();
			pfd.events = POLLIN | POLLPRI;
			if (poll(&pfd, 1, 100) == 0) {
				if (lStopRequested) {
					std::cerr << "stopping cfg change listener thread" << std::endl;
					return;
				}
			}

			if (pfd.revents & (POLLIN | POLLPRI)) {
				pgsql::consumeInput();
				while (auto notification = pgsql::getNotifcation()) {
					auto uid = std::stoi(notification->extra);
					std::cout << "got notification '" << notification->relname << "' " << uid << std::endl;
					if (strcmp(notification->relname, "uid_configs_update") == 0) {
						auto it = lMeasurements.find(uid);
						if (it != lMeasurements.end()) {
							it->second->fConfigure();
						}
					} else if (strcmp(notification->relname, "setvalue_request") == 0) {
						fProcessPendingRequests(uid);
					}
				}
			}
		}
	}

	void daemon::fStartThreads() {
		lThreads.insert(new std::thread(&daemon::fSignalCatcherThread, this));
		lThreads.insert(new std::thread(&daemon::fReaderThread, this));
		lThreads.insert(new std::thread(&daemon::fPollerThread, this));
		lStorerThread = new std::thread(&daemon::fStorerThread, this);
		fClearOldPendingRequests();
		fProcessPendingRequests();
		lThreads.insert(new std::thread(&daemon::fConfigChangeListener, this));
		lThreads.insert(new std::thread(&daemon::fScheduledWriterThread, this));
	}
	void daemon::fAddThread(std::thread *aThread) {
		lThreads.insert(aThread);
	}


	void daemon::fWaitForThreads() {
		for (auto thread : lThreads) {
			thread->join();
		}
		fSignalToStorer();
		lStorerThread->join();
	}

} // end of namespace slowcontrol
