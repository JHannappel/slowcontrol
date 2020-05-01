#include "slowcontrol.h"
#include <iostream>
#include <string.h>
#include <poll.h>
#include <Options.h>
#include "pgsqlWrapper.h"

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
	Only after sparsification data are sent to the database, keeping the load
  low.
 */

namespace slowcontrol {
	std::string base::gHostname;


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
		pgsql::fAddEscapedStringToQuery(aKeyValue, query);
		query += ";";
		pgsql::request result(query);
		if (result.size() != 1) {
			query = "INSERT INTO ";
			query += aTable;
			query += " (";
			query += aKeyColumn;
			if (aExtraColum != nullptr) {
				query += ",";
				query += aExtraColum;
			}
			query += ") VALUES (";
			pgsql::fAddEscapedStringToQuery(aKeyValue, query);
			if (aExtraColum != nullptr) {
				query += ",";
				pgsql::fAddEscapedStringToQuery(aExtraValue, query);
			}
			query += ") RETURNING ";
			query += aIdColumn;
			query += ";";
			result.update(query);
			if (aInsertWasDone != nullptr) {
				*aInsertWasDone = true;
			}
		} else {
			if (aInsertWasDone != nullptr) {
				*aInsertWasDone = false;
			}
		}
		auto id = std::stol(result.getValue(0, 0));
		return id;
	}


	void base::fAddToCompound(int aCompound, uidType aUid, const char* aName) {
		std::string query("SELECT * FROM compound_uids WHERE id=");
		query += std::to_string(aCompound);
		query += " AND uid= ";
		query += std::to_string(aUid);
		query += ";";
		pgsql::request result(query);
		if (result.size() != 1) {
			query = "INSERT INTO compound_uids (id,uid,child_name) VALUES (";
			query += std::to_string(aCompound);
			query += ", ";
			query += std::to_string(aUid);
			query += ", ";
			pgsql::fAddEscapedStringToQuery(aName, query);
			query += ");";
			pgsql::request{query};
		}
	}
	void base::fAddSubCompound(int aParent, int aChild, const char* aName) {
		std::string query("SELECT * FROM compound_families WHERE parent_id=");
		query += std::to_string(aParent);
		query += " AND child_id= ";
		query += std::to_string(aChild);
		query += ";";
		pgsql::request result(query);
		if (result.size() != 1) {
			query = "INSERT INTO compound_families (parent_id,child_id,child_name) VALUES (";
			query += std::to_string(aParent);
			query += ", ";
			query += std::to_string(aChild);
			query += ", ";
			pgsql::fAddEscapedStringToQuery(aName, query);
			query += ");";
			pgsql::request{query};
		}
	}
	void base::fAddToCompound(int aCompound, uidType aUid, const std::string& aName) {
		fAddToCompound(aCompound, aUid, aName.c_str());
	}
	void base::fAddSubCompound(int aParent, int aChild, const std::string& aName) {
		fAddSubCompound(aParent, aChild, aName.c_str());
	}


	bool base::fRequestValueSetting(uidType aUid, const std::string& aRequest,
	                                const std::string& aComment,
	                                std::string& aResponse) {
		pgsql::request("LISTEN setvalue_update;");
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(aUid);
		query += ",";
		pgsql::fAddEscapedStringToQuery(aRequest, query);
		query += ",";
		pgsql::fAddEscapedStringToQuery(aComment, query);
		query += ") RETURNING id;";
		pgsql::request result(query);
		auto id = std::stol(result.getValue(0, 0));
		while (true) {
			struct pollfd pfd;
			pfd.fd = pgsql::getFd();
			pfd.events = POLLIN | POLLPRI;
			poll(&pfd, 1, -1);
			if (pfd.revents & (POLLIN | POLLPRI)) {
				pgsql::consumeInput();
				while (auto notification = pgsql::notification::get()) {
					auto uid = std::stoi(notification->payload());
					std::cout << "got notification '" << notification->channel() << "' " << uid << std::endl;
					if (aUid == uid) {
						query = "SELECT * FROM setvalue_requests WHERE id=";
						query += std::to_string(id);
						query += ";";
						pgsql::request result2(query);
						if (result2.isNull(0, "result")) {
							std::cout << "no result yet" << std::endl;
							break; // spuriuos notification or not yet ready
						}
						aResponse = result2.getValue(0, "response");
						auto outcome = strcmp(result.getValue(0, "result"), "t") == 0;
						std::cout << "result is '" << result.getValue(0, "response") << "'" << std::endl;
						std::cout << "result is now '" << aResponse << "'" << std::endl;
						return (outcome);
					}
				}
			}
		}
	}

} // end of namespace slowcontrol
