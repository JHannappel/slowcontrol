#include "pgsqlWrapper.h"
#include <libpq-fe.h>
#include <Options.h>
#include <map>
#include <string>
#include <thread>

namespace pgsql {
	static options::single<const char*> gDatabaseString('\0', "dataBaseString", "connection info for database");
	static std::map<std::thread::id, PGconn *> gConnections;
	static PGconn *fGetDbconn() {
		auto it = gConnections.find(std::this_thread::get_id());
		if (it == gConnections.end()) {
			unsigned int retries = 0;
			PGconn *dbc = PQconnectdb(gDatabaseString);
			while (PQstatus(dbc) == CONNECTION_BAD) {
				std::cerr << PQerrorMessage(dbc) << std::endl;
				retries = std::min<unsigned int>(retries + 1, 120);
				std::this_thread::sleep_for(std::chrono::seconds(retries));
				std::cerr << "retry to create db connection in " << std::this_thread::get_id() << std::endl;
				dbc = PQconnectdb(gDatabaseString);
			}
			gConnections[std::this_thread::get_id()] = dbc;
			return dbc;
		}
		auto connection = it->second;
		unsigned int retries = 0;
		while (PQstatus(connection) != CONNECTION_OK) {
			std::cerr << PQerrorMessage(connection) << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(retries));

			retries = std::min<unsigned int>(retries + 1, 120);
			std::cerr << "retry to reset db connection in " << std::this_thread::get_id() << std::endl;
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
			std::cerr << PQerrorMessage(conn) << "\n";
		}
	}
	const char* request::getValue(int row, int column) {
		return PQgetvalue(result, row, column);
	}
	const char* request::getValue(int row, const std::string& column) {
		return PQgetvalue(result, row, PQfnumber(result, column.c_str()));
	}
	size_t request::size() const {
		return PQntuples(result);
	}
	void consumeInput() {
		PQconsumeInput(fGetDbconn());
	}

	std::unique_ptr<PGnotify, void(*)(void*)> getNotifcation() {
		return std::unique_ptr<PGnotify, void(*)(void*)>(PQnotifies(fGetDbconn()), PQfreemem);
	};

	void fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery) {
		auto escaped = PQescapeLiteral(fGetDbconn(), aString.c_str(), strlen(aString));
		aQuery += escaped;
		PQfreemem(escaped);
	}


}; // end of pqsql namespace


