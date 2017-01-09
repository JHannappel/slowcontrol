#include "measurement.h"
#include "slowcontrolDaemon.h"
//#include "date.h"
//#include "chrono_io.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
#include <typeinfo>

//using namespace date;

class ruleNode {
  protected:
	static std::map<std::string, ruleNode* (*)(const std::string&, int)>& fGetNodeCreatorMap() {
		static std::map<std::string, ruleNode* (*)(const std::string&, int)> gNodeCreators;
		return gNodeCreators;
	};
	static std::map<std::string, ruleNode*>& fGetNodeMap() {
		static std::map<std::string, ruleNode*> gNodeMap;
		return gNodeMap;
	};
	static std::map<int, ruleNode*>& fGetNodeByIdMap() {
		static std::map<int, ruleNode*> gNodeMap;
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
	static ruleNode* fGetNodeById(int aId) {
		auto it = fGetNodeByIdMap().find(aId);
		if (it != fGetNodeByIdMap().end()) {
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
		fGetNodeByIdMap().emplace(aNodeId, this);
		std::cout << "constructed " << aName << " as " << fGetNodeMap().size() << "th \n";
	};
	virtual void fInit() {
		std::cout << "initializing " << lName << " with " << lConfigValues.size() << "cfgvalues.\n";
		slowcontrol::configValueBase::fConfigure("rule_configs", "nodeid", lNodeId, lConfigValues);
	};
	virtual void fRegisterDependentNode(ruleNode* aNode) {
		lDependentNodes.insert(aNode);
	};
	virtual void fProcess() {
		std::cout << " processing " << lName << "\n";
		for (auto dependentNode : lDependentNodes) {
			std::cout << " going to a child of " << lName << "\n";
			dependentNode->fProcess();
		}
	};
	virtual double fGetValueAsDouble() const = 0;
	virtual bool fGetValueAsBool() const = 0;
	virtual slowcontrol::measurementBase::timeType fGetTime() const {
		return lTime;
	};
	virtual void fSetTime(slowcontrol::measurementBase::timeType aTime) {
		lTime = aTime;
	}
};

class timeableRuleNodeInterface {
  public:
	virtual void fResumeAt(slowcontrol::measurementBase::timeType aThen);
	virtual void fProcessTimed() = 0;
};

class timedActionsClass {
  protected:
	std::multimap<slowcontrol::measurementBase::timeType, timeableRuleNodeInterface*> lTimes;
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

	void fRegisterAction(timeableRuleNodeInterface *aNode,
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
			it->second->fProcessTimed();
			it = lTimes.erase(it);
		}
		return 1000;
	};
};

void timeableRuleNodeInterface::fResumeAt(slowcontrol::measurementBase::timeType aThen) {
	timedActionsClass::fGetInstance()->fRegisterAction(this, aThen);
}


template <typename parentContainer> void fRegisterParent(parentContainer& where, ruleNode* parent, const std::string& slot);
template <> void fRegisterParent<std::multimap<std::string, ruleNode*>>(std::multimap<std::string, ruleNode*>& where, ruleNode* parent, const std::string& slot) {
	where.emplace(slot, parent);
};
template <> void fRegisterParent<std::set<ruleNode*>>(std::set<ruleNode*>& where, ruleNode* parent, const std::string&) {
	where.emplace(parent);
};
template <> void fRegisterParent<ruleNode*>(ruleNode* &where, ruleNode* parent, const std::string&) {
	where = parent;
};

/// templated base class for rule nodes with parents.
/// the first template parameter gives the number of parents,
/// with the special value 0 for arbitrarily many


template <unsigned nParents = 0> class ruleNodeWithParents: public ruleNode {
	class mapDummy {
	  public:
		void emplace(std::string&, ruleNode*) {};
	};
	class setDummy {
	  public:
		void emplace(ruleNode*) {};
	};
  protected:
	typename std::conditional<nParents != 1,
	                          typename std::conditional<nParents != 0,
	                                                    std::multimap<std::string, ruleNode*>,
	                                                    std::set<ruleNode*>
	                                                    >::type,
	                          mapDummy
	                          >::type lParents;
	typename std::conditional < nParents == 1,
	         ruleNode*,
	         void* >::type lParent;
  private:

  public:
	ruleNodeWithParents(const std::string& aName, int aNodeId) :
		ruleNode(aName, aNodeId) {
	};

	template<unsigned n=nParents> 	typename std::enable_if<n >= 1,ruleNode*>::type fSetNamedParent(const std::string& aName) {
		auto it = lParents.find(aName);
		if (it == lParents.end()) {
			throw "required parent not found";
		}
		return it->second;
	};

	template<unsigned n=nParents> 	typename std::enable_if<n >= 1>::type fSetTimeFromParents() {
		for (auto it : lParents) {
			if (lTime < it.second->fGetTime()) {
				fSetTime(it.second->fGetTime());
			}
		}
	};

	virtual void fInit() {
		ruleNode::fInit();
		std::string query("SELECT parent,slot FROM rule_node_parents WHERE nodeid=");
		query += std::to_string(lNodeId);
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
		if (nParents != 0 && nParents != PQntuples(result)) {
			throw "wrong number of parents.";
		};
		for (int i = 0; i < PQntuples(result); ++i) {
			auto parentId = std::stoi(PQgetvalue(result, i, PQfnumber(result, "parent")));
			auto parent = fGetNodeById(parentId);
			parent->fRegisterDependentNode(this);
			if (nParents == 0) {
				fRegisterParent(lParents, parent, "");
			} else if (nParents == 1) {
				fRegisterParent(lParent, parent, "");
			} else {
				std::string slot = PQgetvalue(result, i, PQfnumber(result, "slot"));
				fRegisterParent(lParents, parent, slot);
			}
		}
		PQclear(result);
	};
};


template <unsigned nParents=0> class ruleNodeLogical: public ruleNodeWithParents<nParents> {
  protected:
	bool lValue;
	ruleNodeLogical(const std::string& aName, int aNodeId):
		ruleNodeWithParents<nParents>(aName, aNodeId) {
	}
  public:
	virtual double fGetValueAsDouble() const {
		return lValue ? 1.0 : 0.0;
	}
	virtual bool fGetValueAsBool() const {
		return lValue;
	}
};

class ruleNodeOr: public ruleNodeLogical<> {
protected:
	ruleNodeOr(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeOr(aName, aId);
	};
	virtual void fProcess() {
		lValue = false;
		for (auto parent : lParents) {
			lValue |= parent->fGetValueAsBool();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	}
};

class ruleNodeAnd: public ruleNodeLogical<> {
protected:
	ruleNodeAnd(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeAnd(aName, aId);
	};
	virtual void fProcess() {
		lValue = true;
		for (auto parent : lParents) {
			lValue &= parent->fGetValueAsBool();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	}
};

template <unsigned nParents=0> class ruleNodeArithmetic: public ruleNodeWithParents<nParents> {
  protected:
	double lValue;
  public:
	ruleNodeArithmetic(const std::string& aName, int aNodeId):
		ruleNodeWithParents<nParents>(aName, aNodeId) {
	}
	virtual double fGetValueAsDouble() const {
		return lValue;
	}
	virtual bool fGetValueAsBool() const {
		return lValue != 0.0;
	};
};

class ruleNodeSum: public ruleNodeArithmetic<> {
protected:
	ruleNodeSum(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeSum(aName, aId);
	};

	virtual void fProcess() {
		lValue = 0.0;
		for (auto parent : lParents) {
			lValue += parent->fGetValueAsDouble();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	};
};

class ruleNodeProduct: public ruleNodeArithmetic<> {
protected:
	ruleNodeProduct(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeProduct(aName, aId);
	};

	virtual void fProcess() {
		lValue = 1.0;
		for (auto parent : lParents) {
			lValue *= parent->fGetValueAsDouble();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	};
};

class ruleNodeQuotient: public ruleNodeArithmetic<2> {
protected:
	ruleNode* lDividend;
	ruleNode* lDivisor;
	ruleNodeQuotient(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeQuotient(aName, aId);
	};
	virtual void fInit() {
		ruleNodeArithmetic::fInit();
		lDividend = fSetNamedParent("dividend");
		lDivisor = fSetNamedParent("divisor");
	}

	virtual void fProcess() {
		lValue = lDividend->fGetValueAsDouble() / lDivisor->fGetValueAsDouble();
		fSetTimeFromParents();
		ruleNode::fProcess();
	};
};


class ruleNodeDelay: public ruleNodeWithParents<1>,
	public timeableRuleNodeInterface {
  protected:
	slowcontrol::configValue<std::chrono::system_clock::duration> lDelay;
	double lValue;
  public:
	ruleNodeDelay(const std::string& aName, int aNodeId):
		ruleNodeWithParents(aName, aNodeId),
		lDelay("delay", lConfigValues, std::chrono::seconds(1)) {
	}
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeDelay(aName, aId);
	};

	virtual double fGetValueAsDouble() const {
		return lValue;
	}
	virtual bool fGetValueAsBool() const {
		return lValue != 0.0;
	}
	virtual void fProcess() {
		std::cout << " processing undelayed " << lName << "\n";
		auto then = lParent->fGetTime() + lDelay.fGetValue();
		lValue = lParent->fGetValueAsDouble();
		fResumeAt(then);
	}
	virtual void fProcessTimed() {
		ruleNode::fProcess();
	}
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
	virtual double fGetValueAsDouble() const {
		return lCurrentValue.load();
	};
	virtual bool fGetValueAsBool() const {
		return lCurrentValue.load() != 0;
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
	virtual double fGetValueAsDouble() const {
		return 1.0;
	};
	virtual bool fGetValueAsBool() const {
		return true;
	};
	virtual void fSetFromString(const char* /*aString*/) {
	};
	virtual const char *fGetValueExpression() {
		return " 1 AS value ";
	};
};


class ruleNodeAction: public ruleNodeWithParents<1> {
  protected:
	slowcontrol::base::uidType lUid;
  public:
	ruleNodeAction(const std::string& aName, int aId) :
		ruleNodeWithParents(aName, aId) {
	};
	void fSetUid(slowcontrol::base::uidType aUid) {
		lUid = aUid;
	};
};
template <typename T> class ruleNodeTypedAction: public ruleNodeAction {
  public:
	ruleNodeTypedAction(const std::string& aName, int aId) :
		ruleNodeAction(aName, aId) {
	}
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeTypedAction(aName, aId);
	};
	virtual double fGetValueAsDouble() const {
		return lParent->fGetValueAsDouble();
	};
	virtual bool fGetValueAsBool() const {
		return lParent->fGetValueAsBool();
	};
	virtual void fProcess() {
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(lUid);
		query += ",";
		std::string request("set ");
		request += std::to_string(static_cast<T>(fGetValueAsDouble()));
		slowcontrol::base::fAddEscapedStringToQuery(request, query);
		query += ",'by rule processor');";
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
		PQclear(result);
		ruleNodeAction::fProcess();
	};
};
class ruleNodeFloatAction: public ruleNodeTypedAction<float> {
};
class ruleNodeBoolAction: public ruleNodeTypedAction<bool> {
};
class ruleNodeTriggerAction: ruleNodeAction {
  public:
	ruleNodeTriggerAction(const std::string& aName, int aId) :
		ruleNodeAction(aName, aId) {
	}
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeTriggerAction(aName, aId);
	};
	virtual double fGetValueAsDouble() const {
		return lParent->fGetValueAsDouble();
	};
	virtual bool fGetValueAsBool() const {
		return lParent->fGetValueAsBool();
	};
	virtual void fProcess() {
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(lUid);
		query += ",'set','by rule processor');";
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
		PQclear(result);
		ruleNodeAction::fProcess();
	};
};



template <typename T> class ruleNodeDerivedValue: public ruleNodeWithParents<1>,
                                                  public slowcontrol::boundCheckerInterface<slowcontrol::measurement<T>>
{
 protected:
	ruleNodeDerivedValue(const std::string& aName, int aId) :
		ruleNodeWithParents<1>(aName, aId),
		slowcontrol::boundCheckerInterface<slowcontrol::measurement<T>>(std::numeric_limits<T>::lowest(),std::numeric_limits<T>::max()) {
	};
	virtual void fProcess() {
		this->fStore(lParent->fGetValueAsDouble(),lParent->fGetTime());
		ruleNodeWithParents::fProcess();
	};
	virtual void fInit() {
		ruleNodeWithParents::fInit();
		this->fInitializeUid(lName);
		this->fConfigure();
	};
};

template <> class ruleNodeDerivedValue<bool>: public ruleNodeWithParents<1>,
                                        public slowcontrol::measurement<bool>
{
 protected:
	ruleNodeDerivedValue(const std::string& aName, int aId) :
		ruleNodeWithParents<1>(aName, aId),
		slowcontrol::measurement<bool>() {
	};
	virtual void fProcess() {
		fStore(lParent->fGetValueAsBool(),lParent->fGetTime());
		ruleNodeWithParents::fProcess();
	};
	virtual void fInit() {
		ruleNodeWithParents::fInit();
		fInitializeUid(lName);
		fConfigure();
	};
};

class ruleNodeDerivedFloat : ruleNodeDerivedValue<float> {
protected:
	ruleNodeDerivedFloat(const std::string& aName, int aId) :
		ruleNodeDerivedValue(aName,aId) {
		lClassName.fSetFromString(__func__);
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeDerivedFloat(aName, aId);
	};
	virtual double fGetValueAsDouble() const {
		return const_cast<ruleNodeDerivedFloat*>(this)->fGetCurrentValue();
	};
	virtual bool fGetValueAsBool() const {
		return const_cast<ruleNodeDerivedFloat*>(this)->fGetCurrentValue() != 0.0;
	};
};
class ruleNodeDerivedBool : ruleNodeDerivedValue<bool> {
protected:
	ruleNodeDerivedBool(const std::string& aName, int aId) :
		ruleNodeDerivedValue(aName,aId) {
		lClassName.fSetFromString(__func__);
	};
public:
	static ruleNode* ruleNodeCreator(const std::string& aName, int aId) {
		return new ruleNodeDerivedBool(aName, aId);
	};
	virtual double fGetValueAsDouble() const {
		return const_cast<ruleNodeDerivedBool*>(this)->fGetCurrentValue() ? 1.0 : 0.0;
	};
	virtual bool fGetValueAsBool() const {
		return const_cast<ruleNodeDerivedBool*>(this)->fGetCurrentValue();
	};
};

class dataTable {
  protected:
	std::map<slowcontrol::base::uidType, ruleNodeMeasurement*> lMeasurements;
	std::string lTableName;
	std::string lValueExpression;
	std::string lQuery;
	slowcontrol::measurementBase::timeType lLastQueryTime;
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

		lQuery = "SELECT uid, EXTRACT('epoch' from time AT TIME ZONE 'UTC') AS time, ";
		lQuery += lValueExpression;
		lQuery += " FROM ";
		lQuery += lTableName;
		lQuery += " WHERE uid in (";
		for (auto it = lMeasurements.begin(); it != lMeasurements.end(); ++it) {
			if (it != lMeasurements.begin()) {
				lQuery += ",";
			}
			lQuery += std::to_string(it->first);
		}
		lQuery += ") AND EXTRACT('epoch' from time AT TIME ZONE 'UTC') > ";
		fProcess(false);
	};
	void fProcess(bool aCallProcess = true) {
		std::string query = lQuery;
		query += std::to_string((std::chrono::duration<double, std::milli>(lLastQueryTime.time_since_epoch()).count() + 1) / 1000.);
		query += ";";
		std::cout << query << "\n";
		auto result = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());

		for (int i = 0; i < PQntuples(result); i++) {
			auto uid = std::stoi(PQgetvalue(result, i, PQfnumber(result, "uid")));
			auto seconds = std::stod(PQgetvalue(result, i, PQfnumber(result, "time")));
			slowcontrol::measurementBase::timeType time(std::chrono::microseconds(static_cast<long long>(seconds * 1000000)));
			if (time > lLastQueryTime ) {
				lLastQueryTime = time;
			}
			auto it = lMeasurements.find(uid);
			if (it != lMeasurements.end()) {
				auto measurement = it->second;
				measurement->fSetTime(time);
				measurement->fSetFromString(PQgetvalue(result, i, PQfnumber(result, "value")));
				if (aCallProcess) {
					measurement->fProcess();
				}
			}
		}
		PQclear(result);
	};
};



int main(int argc, const char *argv[]) {
	OptionParser parser("slowcontrol program for processing rules");
	parser.fParse(argc, argv);

	ruleNode::fRegisterNodeTypecreator("or", ruleNodeOr::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("and", ruleNodeAnd::ruleNodeCreator);

	ruleNode::fRegisterNodeTypecreator("sum", ruleNodeSum::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("product", ruleNodeProduct::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("quotient", ruleNodeQuotient::ruleNodeCreator);

	ruleNode::fRegisterNodeTypecreator("delay", ruleNodeDelay::ruleNodeCreator);
	
	ruleNode::fRegisterNodeTypecreator("measurements_float", ruleNodeFloatMeasurement::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("measurements_bool", ruleNodeBoolMeasurement::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("measurements_trigger", ruleNodeTriggerMeasurement::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("actions_float", ruleNodeFloatAction::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("actions_bool", ruleNodeBoolAction::ruleNodeCreator);
	ruleNode::fRegisterNodeTypecreator("actions_trigger", ruleNodeTriggerAction::ruleNodeCreator);

	auto daemon = new slowcontrol::daemon("ruleProcessord");


	std::map<std::string, dataTable*> dataTables;
	{
		auto result = PQexec(slowcontrol::base::fGetDbconn(), "SELECT nodetype, nodename,nodeid FROM rule_nodes;");
		for (int i = 0; i < PQntuples(result); ++i) {
			std::string type(PQgetvalue(result, i, PQfnumber(result, "nodetype")));
			std::string name(PQgetvalue(result, i, PQfnumber(result, "nodename")));
			auto id = std::stoi(PQgetvalue(result, i, PQfnumber(result, "nodeid")));
			bool isMeasurement = type.compare("measurement") == 0;
			bool isAction = type.compare("action") == 0;
			if (isMeasurement || isAction) { // special treatment for measurements

				std::string query("SELECT uid, data_table, is_write_value FROM uid_list WHERE description = ");
				slowcontrol::base::fAddEscapedStringToQuery(name, query);
				query += ";";
				auto res2 = PQexec(slowcontrol::base::fGetDbconn(), query.c_str());
				if (PQntuples(res2) == 1) {
					auto table = PQgetvalue(res2, 0, PQfnumber(res2, "data_table"));
					auto uid = std::stoi(PQgetvalue(res2, 0, PQfnumber(res2, "uid")));
					if (isMeasurement) {
						auto node = ruleNode::fCreateNode(table, name, id);
						auto measurement = dynamic_cast<ruleNodeMeasurement*>(node);
						auto it = dataTables.find(table);
						if (it == dataTables.end()) {
							auto res = dataTables.emplace(table, new dataTable);
							it = res.first;
						}
						it->second->fAddMeasurement(uid, measurement, table, measurement->fGetValueExpression());
					} else { // it must be an action
						std::string detailled_type(table);
						detailled_type.replace(0, strlen("measurements"), "actions");
						auto node = ruleNode::fCreateNode(detailled_type, name, id);
						auto action = dynamic_cast<ruleNodeAction*>(node);
						action->fSetUid(uid);
					}
				} else { // did not find measurement
					std::cout << "no result from " << query << "\n";
					throw "mist";
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
