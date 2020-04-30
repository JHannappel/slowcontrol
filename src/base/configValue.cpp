#include "configValue.h"
#include "slowcontrol.h"
#include <set>
#include <iostream>
#include <OptionsChrono.h>
#include "pgsqlWrapper.h"
namespace slowcontrol {
	void configValueBase::fSave(const char *aTable, const char *aIdColumn, int aId, const char *aComment) const {
		std::string query("INSERT INTO ");
		query += aTable;
		query += " (";
		query += aIdColumn;
		query += ",name,value,comment) VALUES (";
		query += std::to_string(aId);
		query += ",";
		pgsql::fAddEscapedStringToQuery(fGetName(), query);
		query += ",";
		std::string valueRaw;
		fAsString(valueRaw);
		pgsql::fAddEscapedStringToQuery(valueRaw, query);
		query += ",";
		pgsql::fAddEscapedStringToQuery(aComment, query);
		query += ");";
		pgsql::request result(query);
	}
	void configValueBase::fUpdate(const char *aTable, const char *aIdColumn, int aId, const char *aComment) const {
		std::string query = "UPDATE ";
		query += aTable;
		query += " SET value=";
		std::string valueRaw;
		fAsString(valueRaw);
		pgsql::fAddEscapedStringToQuery(valueRaw, query);
		query += ", comment=";
		pgsql::fAddEscapedStringToQuery(aComment, query);
		query += ", last_change=now() WHERE ";
		query += aIdColumn;
		query += "=";
		query += std::to_string(aId);
		query += " AND name=";
		pgsql::fAddEscapedStringToQuery(fGetName(), query);
		query += ";";
		pgsql::request{query};
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
		pgsql::request result(query);
		for (int i = 0; i < result.size(); ++i) {
			std::string name(result.getValue(i, "name"));
			auto it = aMap.find(name);
			if (it != aMap.end()) {
				auto cfgVal = it->second;
				std::string valueRaw;
				auto valueInDb = result.getValue(i, "value");
				cfgVal->fAsString(valueRaw);
				std::string comment(result.getValue(i, "comment"));
				if (comment.size() >= 7
				        && comment.rfind("default") == comment.size() - 7
				        && valueRaw.compare(valueInDb) != 0) {
					cfgVal->fUpdate(aTable, aIdColumn, aId, "changed default");
				} else {
					cfgVal->fSetFromString(valueInDb);
				}
				optionsInDb.emplace(name);
			} else {
				std::cerr << "unknown cfg option '" << name << "' with value '" << result.getValue(i, "value") << "' encountered for " << aIdColumn << " " << aId << std::endl;
			}
		}
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
