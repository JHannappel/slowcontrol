#include "measurement.h"
#include "slowcontrolDaemon.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
#include <typeinfo>

class ruleNode {
  protected:
	static std::map<std::string, ruleNode* (*)(const std::string&, int)>& fGetNodeCreatorMap() {
		static std::map<std::string, ruleNode* (*)(const std::string&, int)> gNodeCreators;
		return gNodeCreators;
	};
	static std::map<std::string, ruleNode*> fGetNodeMap() {
		static std::map<std::string, ruleNode*> gNodeMap;
		return gNodeMap;
	};


	slowcontrol::configValueBase::mapType lConfigValues; ///< map of config values, needed also as parameter for the config value constructors
	slowcontrol::measurementBase::timeType lTime;
	std::string lName;
	std::set<ruleNode*> lDependentNodes;
	int lNodeId;
  public:
	static void fRegisterNodeTypecreator(const std::string& aNodeType,
	                                     ruleNode * (*aCreator)(const std::string&, int)) {
		std::cout << "registering " << aNodeType << " at " << (void*)aCreator << "\n";
		ruleNode::fGetNodeCreatorMap().emplace(aNodeType, aCreator);
	}
	static ruleNode* fCreateNode(const std::string& aNodeType,
	                             const std::string& aName,
	                             int aNodeId) {
		auto it = fGetNodeCreatorMap().find(aNodeType);
		if (it != fGetNodeCreatorMap().end()) {
			return (it->second)(aName, aNodeId);
		}
		std::cout << aNodeType << " not found for " << aName << ";";
		throw std::string("can;t find node creator");
	};

	static ruleNode* fGetNodeByName(const std::string& aName) {
		auto it = fGetNodeMap().find(aName);
		if (it != fGetNodeMap().end()) {
			return it->second;
		}
		return nullptr;
	};
	static void fInitAll() {
		for (auto& it : fGetNodeMap()) {
			it.second->fInit();
		}
	};

	ruleNode(const std::string& aName, int aNodeId):
		lName(aName),
		lNodeId(aNodeId) {
		fGetNodeMap().emplace(aName, this);
	};
	virtual void fInit() {
		slowcontrol::configValueBase::fConfigure("rule_configs", "nodeid", lNodeId, lConfigValues);
	};
	virtual void fRegisterDependentNode(ruleNode* aNode) {
		lDependentNodes.insert(aNode);
	};
	virtual void fProcess() {
		for (auto dependentNode : lDependentNodes) {
			dependentNode->fProcess();
		}
	};
	virtual double fGetValueAsDouble() = 0;
	virtual slowcontrol::measurementBase::timeType fGetTime() const {
		return lTime;
	};
	virtual void fSetTime(slowcontrol::measurementBase::timeType aTime) {
		lTime = aTime;
	}
};



class timedActionsClass {
  protected:
	std::multimap<slowcontrol::measurementBase::timeType, ruleNode*> lTimes;
	timedActionsClass() {
	};
  public:
	static timedActionsClass* fGetInstance() {
		static timedActionsClass* gInstance = nullptr;
		if (gInstance == nullptr) {
			gInstance = new timedActionsClass();
		}
		return gInstance;
	};

	void fRegisterAction(ruleNode *aNode,
	                     slowcontrol::measurementBase::timeType aTime,
	                     bool aDoUpdate = true) {
		if (aDoUpdate) {
			for (auto it = lTimes.begin(); it != lTimes.end();) {
				if (it->second == aNode) { // we do an update (re-trigger)
					it = lTimes.erase(it);
				} else { // there might be more entries to this node but shouldnt
					++it;
				}
			}
		}
		lTimes.emplace(aTime, aNode);
	};
	int fProcess() {
		for (auto it = lTimes.begin(); it != lTimes.end();) {
			auto timeDiff = it->first - std::chrono::system_clock::now();
			if (timeDiff > slowcontrol::measurementBase::durationType::zero()) { // the rest comes maybe next time
				return std::chrono::duration_cast<std::chrono::milliseconds>(timeDiff).count();
			}
			it->second->fProcess();
			it = lTimes.erase(it);
		}
		return 1000;
	};
};




class ruleNodeMeasurement: public ruleNode {
  public:
	ruleNodeMeasurement(const std::string& aName, int aNodeId) :
		ruleNode(aName, aNodeId) {
	};
	virtual void fSetFromString(const char* aString) = 0;
	virtual const char *fGetValueExpression() = 0;
};

template <typename T> class ruleNodeTypedMeasurement: public ruleNodeMeasurement {
  protected:
	std::atomic<T> lCurrentValue;
  public:
	ruleNodeTypedMeasurement(const std::string& aName, int aId) :
		ruleNodeMeasurement(aName, aId) {
	}
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeTypedMeasurement(aName, aId);
	};
	virtual double fGetValueAsDouble() {
		return lCurrentValue.load();
	};
	virtual void fSetFromString(const char* aString) {
		std::stringstream buf(aString);
		T value;
		buf >> value;
		lCurrentValue = value;
	};
	virtual const char *fGetValueExpression() {
		if (typeid(bool) == typeid(T)) {
			return " CAST(value AS INTEGER) AS value ";
		} else {
			return " value ";
		};
	}
};

class ruleNodeFloatMeasurement: public ruleNodeTypedMeasurement<float> {
};
class ruleNodeBoolMeasurement: public ruleNodeTypedMeasurement<bool> {
};

class ruleNodeTriggerMeasurement: public ruleNodeMeasurement {
  public:
	ruleNodeTriggerMeasurement(const std::string& aName, int aId) :
		ruleNodeMeasurement(aName, aId) {
	}
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeTriggerMeasurement(aName, aId);
	};
	virtual double fGetValueAsDouble() {
		return 1.0;
	};
	virtual void fSetFromString(const char* /*aString*/) {
	};
	virtual const char *fGetValueExpression() {
		return " 1 AS value ";
	};
};



class dataTable {
  protected:
	std::map<slowcontrol::base::uidType, ruleNodeMeasurement*> lMeasurements;
	std::string lTableName;
	std::string lValueExpression;
	std::string lQuery;
  public:
	void fAddMeasurement(slowcontrol::base::uidType aUid,
	                     ruleNodeMeasurement *aMeasurement,
	                     const std::string& aTableName,
	                     const std::string& aValueExpression) {
		lMeasurements.emplace(aUid, aMeasurement);
		lTableName = aTableName;
		lValueExpression = aValueExpression;
	};
	void fInit() {
		std::string query("CREATE OR REPLACE RULE ruleProcessorNotify AS ON INSERT TO ");
		query += lTableName;
		query += " DO ALSO NOTIFY ruleProcessor_";
		query += lTableName;
		query += "; LISTEN ruleProcessor_";
		query += lTableName;
		query += ";";
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
		PQclear(result);
	};
	void fProcess() {
		std::string query;
		for (auto it = lMeasurements.begin(); it != lMeasurements.end(); ++it) {
			if (it != lMeasurements.begin()) {
				query += " UNION ";
			}
			auto measurement = it->second;
			query += " (SELECT uid, EXTRACT('epoch' from time AT TIME ZONE 'UTC') AS time, ";
			query += measurement->fGetValueExpression();
			query += " FROM ";
			query += lTableName;
			query += " WHERE uid = ";
			query += std::to_string(it->first);
			query += " AND time > (SELECT TIMESTAMP WITH TIME ZONE 'epoch' + ";
			query += std::to_string(std::chrono::duration<double, std::nano>(measurement->fGetTime().time_since_epoch()).count() / 1E9);
			query += " * INTERVAL '1 second') ORDER BY time DESC limit 1)";
		};
		query += ";";
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
		for (int i = 0; i < PQntuples(result); i++) {
			auto uid = std::stoi(PQgetvalue(result, i, PQfnumber(result, "uid")));
			auto seconds = std::stod(PQgetvalue(result, i, PQfnumber(result, "time")));
			slowcontrol::measurementBase::timeType time(std::chrono::microseconds(static_cast<long int>(seconds * 1000000)));
			auto it = lMeasurements.find(uid);
			if (it != lMeasurements.end()) {
				auto measurement = it->second;
				measurement->fSetTime(time);
				measurement->fSetFromString(PQgetvalue(result, i, PQfnumber(result, "value")));
				measurement->fProcess();
			}
		}
	};
};



int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for processing rules");
	parser.fParse(argc, argv);

	ruleNode::fRegisterNodeTypecreator("measurements_float", ruleNodeFloatMeasurement::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("measurements_bool", ruleNodeBoolMeasurement::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("measurements_trigger", ruleNodeTriggerMeasurement::ruleNodeCreator);

	auto daemon = new slowcontrol::daemon("ruleProcessord");


	std::map<std::string, dataTable*> dataTables;
	{
		auto result = PQexec(slowcontrol::base::fGetDbconn(), "SELECT nodetype, nodename,nodeid FROM rule_nodes;");
		for (int i = 0; i < PQntuples(result); ++i) {
			std::string type(PQgetvalue(result, i, PQfnumber(result, "nodetype")));
			std::string name(PQgetvalue(result, i, PQfnumber(result, "nodename")));
			auto id = std::stoi(PQgetvalue(result, i, PQfnumber(result, "nodeid")));
			if (type.compare("measurement") == 0) { // special treatment for measurements
				std::string query("SELECT uid, data_table, is_write_value FROM uid_list WHERE description = ");
				slowcontrol::base::fAddEscapedStringToQuery(name, query);
				query += ";";
				auto res2 = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
				if (PQntuples(res2) == 1) {
					auto table = PQgetvalue(res2, 0, PQfnumber(res2, "data_table"));
					auto node = ruleNode::fCreateNode(table, name, id);
					auto measurement = dynamic_cast<ruleNodeMeasurement*>(node);
					auto uid = std::stoi(PQgetvalue(res2, 0, PQfnumber(res2, "uid")));
					auto it = dataTables.find(table);
					if (it == dataTables.end()) {
						auto res = dataTables.emplace(table, new dataTable);
						it = res.first;
					}
					it->second->fAddMeasurement(uid, measurement, table, measurement->fGetValueExpression());
				}
			} else { // non-measurement nodes
				ruleNode::fCreateNode(type, name, id);
			}
		}
		PQclear(result);
	}

	ruleNode::fInitAll();
	for (auto& it : dataTables) {
		it.second->fInit();
	}

	timedActionsClass& timedActions(*timedActionsClass::fGetInstance());

	daemon->fStartThreads();

	while (!daemon->fGetStopRequested()) {
		auto nextTimeinMs = timedActions.fProcess();
		nextTimeinMs = std::max(nextTimeinMs, 1000);
		nextTimeinMs = std::min(nextTimeinMs, 10);
		struct pollfd pfd;
		pfd.fd = PQsocket(slowcontrol::base::fGetDbconn());
		pfd.events = POLLIN | POLLPRI;
		if (poll(&pfd, 1, nextTimeinMs) == 0) {
			continue;
		}
		if (pfd.revents & (POLLIN | POLLPRI)) {
			PQconsumeInput(slowcontrol::base::fGetDbconn());
			while (true) {
				auto notification = PQnotifies(slowcontrol::base::fGetDbconn());
				if (notification == nullptr) {
					break;
				}
				std::cout << "got notification '" << notification->relname << "'" << std::endl;
				std::string table(notification->relname + strlen("ruleProcessor_"));
				std::cout << "checking table '" << table << "'\n";
				auto it = dataTables.find(table);
				if (it != dataTables.end()) {
					it->second->fProcess();
				}
				PQfreemem(notification);
			}
		}
	}


	daemon->fWaitForThreads();
}
