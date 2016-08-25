#include "measurement.h"
#include "slowcontrol.h"
#include "slowcontrolDaemon.h"

#include <string.h>

SlowcontrolMeasurementBase::SlowcontrolMeasurementBase():
  lMaxDeltaT("MaxDeltaT",lConfigValues,std::chrono::minutes(5)),
  lReadoutInterval("ReadoutInterval",lConfigValues,std::chrono::seconds(30)),
  lMinValueIndex(0),
  lMaxValueIndex(0) {
};

void SlowcontrolMeasurementBase::fInitializeUid(const std::string& aDescription) {
  bool sendDefaultConfigValues = false;
  lUid = slowcontrol::fSelectOrInsert("uid_list","uid",
				      "description",aDescription.c_str(),
				      "data_table",fGetDefaultTableName(),
				      &sendDefaultConfigValues);
  if (sendDefaultConfigValues) {
    for (auto it : lConfigValues) {
      fSaveOption(*(it.second),"initial default");
    }
  }

  slowcontrolDaemon::fGetInstance()->fRegisterMeasurement(this);
};

bool SlowcontrolMeasurementBase::fHasDefaultReadFunction() const {
  return false;
};

void SlowcontrolMeasurementBase::fSaveOption(const configValueBase& aCfgValue,
					     const char *aComment) {
  std::string query("INSERT INTO uid_configs (uid,name,value,comment) VALUES (");
  query += std::to_string(fGetUid());
  query += ",";
  auto name = PQescapeLiteral(slowcontrol::fGetDbconn(), 
			      aCfgValue.fGetName().c_str(), aCfgValue.fGetName().size());
  query += name;
  PQfreemem(name);
  query += ",";
  aCfgValue.fAsString(query);
  query += ",";
  auto comment = PQescapeLiteral(slowcontrol::fGetDbconn(), 
				 aComment, strlen(aComment));
  query += comment;
  PQfreemem(comment);
  query += ");";
  std::cout << query << std::endl;
  auto result=PQexec(slowcontrol::fGetDbconn(), query.c_str());
  PQclear(result);
}

void SlowcontrolMeasurementBase::fConfigure() {
  std::string query("SELECT name,value FROM uid_configs WHERE uid=");
  query+=std::to_string(fGetUid());
  query+=";";
  std::set<std::string> optionsInDb;
  auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
  for (int i=0; i<PQntuples(result); ++i) {
    std::string name(PQgetvalue(result,i,PQfnumber(result,"name")));
    auto it = lConfigValues.find(name);
    if (it != lConfigValues.end()) {
      auto cfgVal = it->second;
      cfgVal->fSetFromString(PQgetvalue(result,i,PQfnumber(result,"value")));
      optionsInDb.emplace(name);
    } else {
      std::cerr << "unknown cfg option '" << name << "' with value '" << PQgetvalue(result,i,PQfnumber(result,"value")) << "' encountered for uid " << fGetUid() << std::endl;
    }
  }
  PQclear(result);
  for (auto it: lConfigValues) {
    auto name = it.first;
    if (optionsInDb.find(name) == optionsInDb.end()) {
      fSaveOption(*(it.second),"later default");
    }
  }

};


void SlowcontrolMeasurementFloat::fSendValue(const timedValue& aValue) {
  std::string query("INSERT INTO ");
  query+=fGetDefaultTableName();
  query+=" (uid, time, value) VALUES ( ";
  query+=std::to_string(fGetUid());
  query+=", (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
  query+=std::to_string(std::chrono::duration<double,std::nano>(aValue.lTime.time_since_epoch()).count()/1E9);
  query+=" * INTERVAL '1 second'),";
  query+=std::to_string(aValue.lValue);
  query+=" );";
  auto result = PQexec(slowcontrol::fGetDbconn(), query.c_str());
PQclear(result);
};
