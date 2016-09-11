#include "slowcontrol.h"
#include <iostream>
#include <string.h>
#include <poll.h>
#include <Options.h>

/*! \mainpage
	A slowcontrol system for general use, eg. in home automation.
	Written in C++ 11, it uses a postgres server as database backend.

	This is work in progress, so even basic parts may change at any time.
	The project servers not only the purpose
	of creating an useful slowcontrol
	but also as programming exercise and playground for concepts.

	It is in a way a re-write of the slowcontrol system that is used by
	the BGO-OD experiment and uses similar basic structures.
	The experience from there shows that the system is scaleable over
	a wide range:
	At a typical home setup a raspberry pi 3 with an USB hard disk
  as database and web server is ample for a few hundred measurements,
  (as shown ny my onwn installation).
	AT BGO-OD the database server is larger and serves more than 15000
  measurements without performance problems.

	The main trick is that the measurements are done by a multitude of
  independent daemons each of which tackles a reasonable task, usually
  conected to one piece of hardware.
	Only after spasification data are sent to the database, keeping the load
  low.
 */

namespace slowcontrol {
	std::map<std::thread::id, PGconn *> base::gConnections;
	std::string base::gHostname;

	static Option<const char*> gDatabaseString('\0', "dataBaseString", "connection info for database");

	PGconn *base::fGetDbconn() {
		auto it = gConnections.find(std::this_thread::get_id());
		if (it == gConnections.end()) {
			unsigned int retries = 0;
			PGconn *dbc = PQconnectdb(gDatabaseString);
			while (PQstatus(dbc) == CONNECTION_BAD) {
				std::cerr << PQerrorMessage(dbc) << std::endl;
				retries = std::min<unsigned int>(retries + 1, 120);
				sleep(retries);
				std::cerr << "retry to create db connection in " << std::this_thread::get_id() << std::endl;
				dbc = PQconnectdb(gDatabaseString);
			}
			gConnections[std::this_thread::get_id()] = dbc;
			return dbc;
		} else {
			auto connection = it->second;
			unsigned int retries = 0;
			while (PQstatus(connection) != CONNECTION_OK) {
				std::cerr << PQerrorMessage(connection) << std::endl;
				sleep(retries);
				retries = std::min<unsigned int>(retries + 1, 120);
				std::cerr << "retry to reset db connection in " << std::this_thread::get_id() << std::endl;
				PQreset(connection);
			}
			return connection;
		}
	}
	const std::string& base::fGetHostName() {
		if (gHostname.empty()) {
			char buf[256];
			gethostname(buf, sizeof(buf));
			gHostname = buf;
		}
		return gHostname;
	}
	int base::fGetCompoundId(const char* aName, const char *aDescription) {
		return fSelectOrInsert("compound_list", "id", "name", aName,
		                       aDescription != nullptr ? "description" : nullptr, aDescription);
	}
	int base::fSelectOrInsert(const char *aTable, const char *aIdColumn,
	                          const char *aKeyColumn, const char *aKeyValue,
	                          const char *aExtraColum, const char *aExtraValue,
	                          bool *aInsertWasDone) {
		std::string query("SELECT ");
		query += aIdColumn;
		query += " FROM ";
		query += aTable;
		query += " WHERE ";
		query += aKeyColumn;
		query += " = ";
		auto keyValue = PQescapeLiteral(base::fGetDbconn(), aKeyValue, strlen(aKeyValue));
		query += keyValue;
		query += ";";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		if (PQntuples(result) != 1) {
			PQclear(result);
			query = "INSERT INTO ";
			query += aTable;
			query += " (";
			query += aKeyColumn;
			if (aExtraColum != nullptr) {
				query += ",";
				query += aExtraColum;
			}
			query += ") VALUES (";
			query += keyValue;
			if (aExtraColum != nullptr) {
				query += ",";
				fAddEscapedStringToQuery(aExtraValue, query);
			}
			query += ") RETURNING ";
			query += aIdColumn;
			query += ";";
			result = PQexec(base::fGetDbconn(), query.c_str());
			if (aInsertWasDone != nullptr) {
				*aInsertWasDone = true;
			}
		} else {
			if (aInsertWasDone != nullptr) {
				*aInsertWasDone = false;
			}
		}
		PQfreemem(keyValue);
		auto id = std::stol(PQgetvalue(result, 0, 0));
		PQclear(result);
		return id;
	}


	void base::fAddToCompound(int aCompound, int aUid, const char* aName) {
		std::string query("SELECT * FROM compound_uids WHERE id=");
		query += std::to_string(aCompound);
		query += " AND uid= ";
		query += std::to_string(aUid);
		query += ";";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		if (PQntuples(result) != 1) {
			PQclear(result);
			query = "INSERT INTO compound_uids (id,uid,child_name) VALUES (";
			query += std::to_string(aCompound);
			query += ", ";
			query += std::to_string(aUid);
			query += ", ";
			fAddEscapedStringToQuery(aName, query);
			query += ");";
			result = PQexec(base::fGetDbconn(), query.c_str());
		}
		PQclear(result);
	}
	void base::fAddSubCompound(int aParent, int aChild, const char* aName) {
		std::string query("SELECT * FROM compound_families WHERE parent_id=");
		query += std::to_string(aParent);
		query += " AND child_id= ";
		query += std::to_string(aChild);
		query += ";";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		if (PQntuples(result) != 1) {
			PQclear(result);
			query = "INSERT INTO compound_families (parent_id,child_id,child_name) VALUES (";
			query += std::to_string(aParent);
			query += ", ";
			query += std::to_string(aChild);
			query += ", ";
			fAddEscapedStringToQuery(aName, query);
			query += ");";
			result = PQexec(base::fGetDbconn(), query.c_str());
		}
		PQclear(result);
	}
	void base::fAddToCompound(int aCompound, int aUid, const std::string& aName) {
		fAddToCompound(aCompound, aUid, aName.c_str());
	}
	void base::fAddSubCompound(int aParent, int aChild, const std::string& aName) {
		fAddSubCompound(aParent, aChild, aName.c_str());
	}


	void base::fAddEscapedStringToQuery(const char *aString, std::string& aQuery) {
		auto escaped = PQescapeLiteral(fGetDbconn(), aString, strlen(aString));
		aQuery += escaped;
		PQfreemem(escaped);
	}
	void base::fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery) {
		fAddEscapedStringToQuery(aString.c_str(), aQuery);
	}

	bool base::fRequestValueSetting(uidType aUid, const std::string& aRequest,
	                                const std::string& aComment,
	                                std::string& aResponse) {
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(aUid);
		query += ",";
		fAddEscapedStringToQuery(aRequest, query);
		query += ",";
		fAddEscapedStringToQuery(aComment, query);
		query += ") RETURNING id;";
		auto result = PQexec(fGetDbconn(), query.c_str());
		auto id = std::stol(PQgetvalue(result, 0, 0));
		PQclear(result);
		result = PQexec(fGetDbconn(), "LISTEN setvalue_update;");
		if (PQresultStatus(result) != PGRES_COMMAND_OK) {
			std::cerr << "LISTEN command failed" <<  PQerrorMessage(base::fGetDbconn()) << std::endl;
			PQclear(result);
			return false;
		}
		PQclear(result);
		while (true) {
			struct pollfd pfd;
			pfd.fd = PQsocket(base::fGetDbconn());
			pfd.events = POLLIN | POLLPRI;
			poll(&pfd, 1, -1);
			if (pfd.revents & (POLLIN | POLLPRI)) {
				PQconsumeInput(base::fGetDbconn());
				while (true) {
					auto notification = PQnotifies(base::fGetDbconn());
					if (notification == nullptr) {
						break;
					}
					std::cout << "got notification '" << notification->relname << "'" << std::endl;
					if (strcmp(notification->relname, "setvalue_update") == 0) {
						PQfreemem(notification);
						query = "SELECT * FROM setvalue_requests WHERE id=";
						query += std::to_string(id);
						query += ";";
						result = PQexec(fGetDbconn(), query.c_str());
						if (PQgetisnull(result, 0, PQfnumber(result, "result")) != 0) {
							PQclear(result);
							std::cout << "no result yet" << std::endl;
							break; // spuriuos notification or not yet ready
						}
						aResponse = PQgetvalue(result, 0, PQfnumber(result, "response"));
						auto outcome = strcmp(PQgetvalue(result, 0, PQfnumber(result, "result")), "t") == 0;
						std::cout << "result is '" << PQgetvalue(result, 0, PQfnumber(result, "response")) << "'" << std::endl;
						PQclear(result);
						std::cout << "result is now '" << aResponse << "'" << std::endl;
						return (outcome);
					} else {
						PQfreemem(notification);
					}
				}
			}

		}
	}

} // end of namespace slowcontrol
