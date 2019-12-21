#include "configValue.h"
#include "slowcontrol.h"
#include <set>
#include <iostream>
#include <OptionsChrono.h>

namespace slowcontrol {
	void configValueBase::fSave(const char *aTable, const char *aIdColumn, int aId, const char *aComment) const {
		std::string query("INSERT INTO ");
		query += aTable;
		query += " (";
		query += aIdColumn;
		query += ",name,value,comment) VALUES (";
		query += std::to_string(aId);
		query += ",";
		base::fAddEscapedStringToQuery(fGetName(), query);
		query += ",";
		std::string valueRaw;
		fAsString(valueRaw);
		base::fAddEscapedStringToQuery(valueRaw, query);
		query += ",";
		base::fAddEscapedStringToQuery(aComment, query);
		query += ");";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		PQclear(result);
	}
	void configValueBase::fUpdate(const char *aTable, const char *aIdColumn, int aId, const char *aComment) const {
		std::string query = "UPDATE ";
		query += aTable;
		query += " SET value=";
		std::string valueRaw;
		fAsString(valueRaw);
		base::fAddEscapedStringToQuery(valueRaw, query);
		query += ", comment=";
		base::fAddEscapedStringToQuery(aComment, query);
		query += ", last_change=now() WHERE ";
		query += aIdColumn;
		query += "=";
		query += std::to_string(aId);
		query += " AND name=";
		base::fAddEscapedStringToQuery(fGetName(), query);
		query += ";";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		PQclear(result);
	}
	void configValueBase::fConfigure(const char *aTable, const char *aIdColumn, int aId, mapType& aMap) {
		std::string query("SELECT name,value,comment FROM ");
		query += aTable;
		query += " WHERE ";
		query += aIdColumn;
		query += "=";
		query += std::to_string(aId);
		query += ";";
		std::set<std::string> optionsInDb;
		std::cout << query << "\n";
		auto result = PQexec(base::fGetDbconn(), query.c_str());
		for (int i = 0; i < PQntuples(result); ++i) {
			std::string name(PQgetvalue(result, i, PQfnumber(result, "name")));
			auto it = aMap.find(name);
			if (it != aMap.end()) {
				auto cfgVal = it->second;
				std::string valueRaw;
				auto valueInDb = PQgetvalue(result, i, PQfnumber(result, "value"));
				cfgVal->fAsString(valueRaw);
				std::string comment(PQgetvalue(result, i, PQfnumber(result, "comment")));
				if (comment.ends_with("default")
				        && valueRaw.compare(valueInDb) != 0) {
					cfgVal->fUpdate(aTable, aIdColumn, aId, "changed default");
				} else {
					cfgVal->fSetFromString(valueInDb);
				}
				optionsInDb.emplace(name);
			} else {
				std::cerr << "unknown cfg option '" << name << "' with value '" << PQgetvalue(result, i, PQfnumber(result, "value")) << "' encountered for " << aIdColumn << " " << aId << std::endl;
			}
		}
		PQclear(result);
		for (auto it : aMap) {
			auto name = it.first;
			if (optionsInDb.find(name) == optionsInDb.end()) {
				it.second->fSave(aTable, aIdColumn, aId, "later default");
			}
		}
	}
	void configValue<std::chrono::system_clock::duration>::fSetFromString(const char *aString) {
		std::chrono::system_clock::duration dur; // buffer needed due to atomicity of lValue
		options::internal::parseDurationString(dur, aString);
		lValue = dur;
	}
	void configValue<std::chrono::system_clock::duration>::fAsString(std::string& aString) const {
		auto a = std::to_string(std::chrono::duration_cast<std::chrono::duration<double>>(lValue.load()).count());
		aString += a;
		aString += " s";
	}
} // namespace slowcontrol
