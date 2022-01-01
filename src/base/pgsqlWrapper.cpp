#include "pgsqlWrapper.h"
#include <libpq-fe.h>
#include <Options.h>
#include <map>
#include <string>
#include <thread>
#include <errMsgQueue.h>

namespace pgsql {
	static options::single<const char*> gDatabaseString('\0', "dataBaseString", "connection info for database");
	static std::map<std::thread::id, PGconn *> gConnections;
	static PGconn *fGetDbconn() {
		auto it = gConnections.find(std::this_thread::get_id());
		if (it == gConnections.end()) {
			unsigned int retries = 0;
			PGconn *dbc = PQconnectdb(gDatabaseString);
			while (PQstatus(dbc) == CONNECTION_BAD) {
			  errMsg::emit(errMsg::level::debug,errMsg::location(),
				       gDatabaseString.fGetValue(), "PQconnectdb ", PQerrorMessage(dbc));
				retries = std::min<unsigned int>(retries + 1, 120);
				std::this_thread::sleep_for(std::chrono::seconds(retries));
				errMsg::emit(errMsg::level::debug,errMsg::location(),
						  "connection","retry");
				dbc = PQconnectdb(gDatabaseString);
			}
			gConnections[std::this_thread::get_id()] = dbc;
			return dbc;
		}
		auto connection = it->second;
		unsigned int retries = 0;
		while (PQstatus(connection) != CONNECTION_OK) {
		  errMsg::emit(errMsg::level::warning,errMsg::location(),
			       gDatabaseString.fGetValue(),"status", PQerrorMessage(connection));
		  std::this_thread::sleep_for(std::chrono::seconds(retries));

		  retries = std::min<unsigned int>(retries + 1, 120);
		  PQreset(connection);
		}
		return connection;
	}

	request::request(const std::string& command): result(nullptr) {
		update(command);
	}
	request::~request() {
		if (result) {
			PQclear(result);
		}
	}
	void request::update(const std::string& command) {
		if (result) {
			PQclear(result);
		}
		result = PQexec(fGetDbconn(), command.c_str());
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		  errMsg::emit(errMsg::level::warning,errMsg::location(),
				    command, "exec", PQerrorMessage(fGetDbconn()));
		}
	}
	const char* request::getValue(int row, int column) {
		return PQgetvalue(result, row, column);
	}
	const char* request::getValue(int row, const std::string& column) {
		return PQgetvalue(result, row, PQfnumber(result, column.c_str()));
	}
	bool request::isNull(int row, const std::string& column) {
		return PQgetisnull(result, row, PQfnumber(result, column.c_str())) != 0;
	}
	size_t request::size() const {
		return PQntuples(result);
	}
	int getFd() {
		return PQsocket(fGetDbconn());
	}
	void consumeInput() {
		PQconsumeInput(fGetDbconn());
	}

	notification::notification(pgNotify *aNotify): notify(aNotify) {};
	notification::~notification() {
		if (notify) {
			PQfreemem(notify);
		}
	}
	std::unique_ptr<notification> notification::get() {
		auto note = PQnotifies(fGetDbconn());
		if (note != nullptr) {
			return std::unique_ptr<notification>(new notification(note));
		} else {
			return nullptr;
		}
	};
	const char* notification::channel() const {
		return notify->relname;
	}
	const char* notification::payload() const {
		return notify->extra;
	}

	void fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery) {
		auto escaped = PQescapeLiteral(fGetDbconn(), aString.c_str(), aString.size());
		aQuery += escaped;
		PQfreemem(escaped);
	}


}; // end of pqsql namespace


