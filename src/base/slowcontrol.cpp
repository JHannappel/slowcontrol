#include "slowcontrol.h"
#include <iostream>
#include <string.h>
#include <Options.h>
std::map<std::thread::id, PGconn *> slowcontrol::gConnections;
std::string slowcontrol::gHostname;

static Option<const char*> gDatabaseString('\0',"dataBaseString","connection info for database");

PGconn *slowcontrol::fGetDbconn() {
  auto it = gConnections.find(std::this_thread::get_id());
  if (it == gConnections.end()) {
    unsigned int retries = 0;
    PGconn *dbc = PQconnectdb(gDatabaseString);
    while (PQstatus(dbc) == CONNECTION_BAD) {
      std::cerr << PQerrorMessage(dbc) << std::endl;
      retries = std::min<unsigned int>(retries+1, 120);
      sleep(retries);
      std::cerr << "retry to create db connection in " << std::this_thread::get_id() << std::endl;
      dbc = PQconnectdb("host=raspberrypi dbname=testdb user=hannappe");
    }
    gConnections[std::this_thread::get_id()] = dbc;
    return dbc;
  } else {
    auto connection = it->second;
    unsigned int retries = 0;
    while (PQstatus(connection) != CONNECTION_OK) {
      std::cerr << PQerrorMessage(connection) << std::endl;
      sleep(retries);
      retries = std::min<unsigned int>(retries+1, 120);
      std::cerr << "retry to reset db connection in " << std::this_thread::get_id() << std::endl;
      PQreset(connection);
    }
    return connection;
  }
}
const std::string& slowcontrol::fGetHostName() {
  if (gHostname.empty()) {
    char buf[256];
    gethostname(buf,sizeof(buf));
    gHostname = buf;
  }
  return gHostname;
}
int slowcontrol::fGetCompoundId(const char* aName, const char *aDescription) {
  return fSelectOrInsert("compound_list","id","name",aName,
			 aDescription != nullptr ? "description" : nullptr,aDescription);
}
int slowcontrol::fSelectOrInsert(const char *aTable, const char *aIdColumn,
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
  auto keyValue=PQescapeLiteral(slowcontrol::fGetDbconn(),aKeyValue,strlen(aKeyValue));
  query += keyValue;
  query += ";";
  auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
  std::cerr << query << " gave " << PQresStatus(PQresultStatus(result)) << std::endl;
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
      auto extraValue = PQescapeLiteral(slowcontrol::fGetDbconn(),aExtraValue,strlen(aExtraValue));
      query += extraValue;
      PQfreemem(extraValue);
    }
    query += ") RETURNING ";
    query += aIdColumn;
    query += ";";
    result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
    std::cerr << query << " gave " << PQresStatus(PQresultStatus(result)) << std::endl;

    if (aInsertWasDone != nullptr) {
      *aInsertWasDone = true;
    }
  } else {
    if (aInsertWasDone != nullptr) {
      *aInsertWasDone = false;
    }
  }
  PQfreemem(keyValue);
  auto id = std::stol(PQgetvalue(result,0,0));
  PQclear(result);
  return id;
}


void slowcontrol::fAddToCompound(int aCompound, int aUid, const char* aName) {
  std::string query("SELECT * FROM compound_uids WHERE id=");
  query += std::to_string(aCompound);
  query += " AND uid= ";
  query += std::to_string(aUid);
  query += ";";
  auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
  if (PQntuples(result) != 1) {
    PQclear(result);
    query = "INSERT INTO compound_uids (id,uid,child_name) VALUES (";
    query += std::to_string(aCompound);
    query += ", ";
    query += std::to_string(aUid);
    query += ", ";
    auto name = PQescapeLiteral(slowcontrol::fGetDbconn(), aName, strlen(aName));
    query += name;
    query += ");";
    result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
    PQfreemem(name);
  }
  PQclear(result);
}
void slowcontrol::fAddSubCompound(int aParent, int aChild, const char* aName) {
  std::string query("SELECT * FROM compound_families WHERE parent_id=");
  query += std::to_string(aParent);
  query += " AND child_id= ";
  query += std::to_string(aChild);
  query += ";";
  auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
  if (PQntuples(result) != 1) {
    PQclear(result);
    query = "INSERT INTO compound_families (parent_id,child_id,child_name) VALUES (";
    query += std::to_string(aParent);
    query += ", ";
    query += std::to_string(aChild);
    query += ", ";
    auto name = PQescapeLiteral(slowcontrol::fGetDbconn(), aName, strlen(aName));
    query += name;
    query += ");";
    result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
    PQfreemem(name);
  }
  PQclear(result);
}
