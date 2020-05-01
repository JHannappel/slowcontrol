#include "measurement.h"
#include "slowcontrolDaemon.h"
//#include "date.h"
//#include "chrono_io.h"
#include <fstream>
#include <Options.h>
#include <stdio.h>
#include <string.h>
#include <typeinfo>
#include "pgsqlWrapper.h"


#define debugthis std::cerr << __FILE__ << ":" << __LINE__ << ": " << __func__ << "@" << (void*)this << ", a " << typeid(*this).name() << "\n"
#define debugthat(that) std::cerr << __FILE__ << ":" << __LINE__ << ": " << __func__ << " on " << (void*)(that) << ", a " << typeid(*(that)).name() << "\n"

//using namespace date;
class ruleNode {
  public:
	class ruleNodeCreator {
	  protected:
		int lMaxParents = -1;
	  public:
		virtual ruleNode *fCreate(const std::string& aName,
		                          int aNodeId) = 0;
		virtual int fGetNumberOfPossibleParents() const = 0;
	};
	template <typename T, typename genT> class ruleNodeCreatorTemplate: public genT::ruleNodeCreator {
	  private:
		template <class t> typename std::enable_if < !std::is_abstract<t>::value, t >::type* fNewT(const std::string& aName,
		        int aNodeId) {
			return new t(aName, aNodeId);
		};
		template <class t> typename std::enable_if<std::is_abstract<t>::value, t>::type* fNewT(const std::string& /*aName*/,
		        int /*aNodeId*/) {
			return nullptr;
		};
	  public:
		ruleNodeCreatorTemplate() {
			debugthis;
		};
		ruleNode* fCreate(const std::string& aName,
		                  int aNodeId) override {
			auto rule = fNewT<T>(aName, aNodeId);
			debugthat(rule);
			return rule;
		};
		static ruleNode::ruleNodeCreator *fGetCreator() {
			static ruleNodeCreatorTemplate gCreator;
			return &gCreator;
		};
		int fGetNumberOfPossibleParents() const override {
			debugthis;
			return this->lMaxParents;
		};
	};

  protected:
	static std::map<std::string, ruleNodeCreator*>& fGetNodeCreatorMap() {
		static std::map<std::string, ruleNodeCreator*> gNodeCreators;
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
	static void fRegisterNodeTypeCreator(const std::string& aNodeType,
	                                     ruleNodeCreator* aCreator) {
		std::cout << "registering " << aNodeType << " at " << reinterpret_cast<void*>(aCreator) << "of type " << typeid(*aCreator).name() << "\n";
		ruleNode::fGetNodeCreatorMap().emplace(aNodeType, aCreator);

		bool isNew = true;
		auto id = slowcontrol::base::fSelectOrInsert("node_types", "typeid",
		          "type", aNodeType.c_str(),
		          nullptr, nullptr, &isNew);
		if (isNew) {
			std::string query("UPDATE node_types SET nparents=");
			query += std::to_string(aCreator->fGetNumberOfPossibleParents());
			query += " WHERE typeid=";
			query += std::to_string(id);
			query += ";";

			pgsql::request{query};
		};
	};
	template <typename T> static void fRegisterNodeTypeCreator(const std::string& aNodeType) {
		fRegisterNodeTypeCreator(aNodeType, T::ruleNodeCreator::fGetCreator());
	}
	static ruleNode* fCreateNode(const std::string& aNodeType,
	                             const std::string& aName,
	                             int aNodeId) {
		auto it = fGetNodeCreatorMap().find(aNodeType);
		if (it != fGetNodeCreatorMap().end()) {
			return it->second->fCreate(aName, aNodeId);
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
template <> void fRegisterParent<std::set<ruleNode*>>(std::set<ruleNode*>& where, ruleNode* parent, const std::string& /*unused*/) {
	where.emplace(parent);
};
template <> void fRegisterParent<ruleNode*>(ruleNode* &where, ruleNode* parent, const std::string& /*unused*/) {
	where = parent;
};


/// templated base class for rule nodes with parents.
/// the first template parameter gives the number of parents,
/// with the special value 0 for arbitrarily many


template <unsigned nParents = 0> class ruleNodeWithParents: public ruleNode {
	class mapDummy {
	  public:
		void emplace(std::string& /*unused*/, ruleNode* /*unused*/) {};
	};
	class setDummy {
	  public:
		void emplace(ruleNode* /*unused*/) {};
	};
  protected:
	typename std::conditional < nParents != 1,
	         typename std::conditional < nParents != 0,
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
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeWithParents<nParents>, ruleNode> {
	  public:
		ruleNodeCreator() {
			debugthis;
			this->lMaxParents = nParents;
		}
		int fGetNumberOfPossibleParents() const override {
			debugthis;
			return nParents;
		};
	};
	ruleNodeWithParents(const std::string& aName, int aNodeId) :
		ruleNode(aName, aNodeId) {
	};

	template<unsigned n = nParents> 	typename std::enable_if<n >= 1, ruleNode* >::type fSetNamedParent(const std::string& aName) {
		auto it = lParents.find(aName);
		if (it == lParents.end()) {
			throw "required parent not found";
		}
		return it->second;
	};

	template<unsigned n = nParents> 	typename std::enable_if<n >= 1 >::type fSetTimeFromParents() {
		for (auto it : lParents) {
			if (lTime < it.second->fGetTime()) {
				fSetTime(it.second->fGetTime());
			}
		}
	};

	void fInit() override {
		ruleNode::fInit();
		std::string query("SELECT parent,slot FROM rule_node_parents WHERE nodeid=");
		query += std::to_string(lNodeId);
		pgsql::request result(query);
		if (nParents != 0 && nParents != result.size()) {
			throw "wrong number of parents.";
		};
		for (unsigned int i = 0; i < result.size(); ++i) {
			auto parentId = std::stoi(result.getValue(i, "parent"));
			auto parent = fGetNodeById(parentId);
			parent->fRegisterDependentNode(this);
			if (nParents == 0) {
				fRegisterParent(lParents, parent, "");
			} else if (nParents == 1) {
				fRegisterParent(lParent, parent, "");
			} else {
				std::string slot(result.getValue(i, "slot"));
				fRegisterParent(lParents, parent, slot);
			}
		}
	};
};


template <unsigned nParents = 0> class ruleNodeLogical: public ruleNodeWithParents<nParents> {
  protected:
	bool lValue;
  public:
	ruleNodeLogical(const std::string& aName, int aNodeId):
		ruleNodeWithParents<nParents>(aName, aNodeId) {
	}
	//class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeLogical, ruleNodeWithParents<nParents>> {};
	double fGetValueAsDouble() const override {
		return lValue ? 1.0 : 0.0;
	}
	bool fGetValueAsBool() const override {
		return lValue;
	}
};

class ruleNodeOr: public ruleNodeLogical<> {
  public:
	ruleNodeOr(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeOr, ruleNodeLogical<>> {
	};
	void fProcess() override {
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
  public:
	ruleNodeAnd(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeAnd, ruleNodeLogical<>> {
	};
	void fProcess() override {
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

class ruleNodeOddNTrue: public ruleNodeLogical<> {
  public:
	ruleNodeOddNTrue(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeOddNTrue, ruleNodeLogical<>> {
	};
	void fProcess() override {
		lValue = false;
		for (auto parent : lParents) {
			lValue ^= parent->fGetValueAsBool();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	}
};
class ruleNodeEvenNTrue: public ruleNodeLogical<> {
  public:
	ruleNodeEvenNTrue(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeEvenNTrue, ruleNodeLogical<>> {
	};
	void fProcess() override {
		lValue = true;
		for (auto parent : lParents) {
			lValue ^= parent->fGetValueAsBool();
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		ruleNode::fProcess();
	}
};

class ruleNodeLatch: public ruleNodeLogical<2> {
  protected:
	ruleNode* lSet;
	ruleNode* lReset;
  public:
	ruleNodeLatch(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeLatch, ruleNodeLogical<2>> {
	};
	void fInit() override {
		ruleNodeLogical::fInit();
		lSet = fSetNamedParent("set");
		lReset = fSetNamedParent("reset");
	}
	void fProcess() override {
		if (lSet->fGetValueAsBool() && lSet->fGetTime() > lReset->fGetTime()) {
			lValue = true;
			fSetTime(lSet->fGetTime());
			ruleNode::fProcess();
		} else if (lReset->fGetValueAsBool() && lReset->fGetTime() > lSet->fGetTime()) {
			lValue = false;
			fSetTime(lReset->fGetTime());
			ruleNode::fProcess();
		}
	}
};



class ruleNodeGreater: public ruleNodeLogical<2> {
  protected:
	ruleNode* lLeft;
	ruleNode* lRight;
  public:
	ruleNodeGreater(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeGreater, ruleNodeLogical<2>> {
	};
	void fInit() override {
		ruleNodeLogical::fInit();
		lLeft = fSetNamedParent("left");
		lRight = fSetNamedParent("right");
	}
	void fProcess() override {
		lValue = lLeft->fGetValueAsDouble() > lRight->fGetValueAsDouble();
		fSetTimeFromParents();
		ruleNode::fProcess();
	}
};

class ruleNodeEqual: public ruleNodeLogical<> {
  protected:
	slowcontrol::configValue<double> lMaxSigma;
  public:
	ruleNodeEqual(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId),
		lMaxSigma("maxsigma", lConfigValues, 0.0) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeEqual, ruleNodeLogical<>> {
	};
	void fProcess() override {
		lValue = true;
		// algorithm from https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
		size_t n = 0;
		double mean = 0.0;
		double M2 = 0.0;
		for (auto parent : lParents) {
			n++;
			auto x = parent->fGetValueAsDouble();
			auto delta = x - mean;
			mean += delta / n;
			auto delta2 = x - mean;
			M2 += delta * delta2;
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
			}
		}
		lValue = M2 / (n - 1) < lMaxSigma;
		ruleNode::fProcess();
	}
};


class ruleNodeLatest: public ruleNodeLogical<> {
  public:
	ruleNodeLatest(const std::string& aName, int aNodeId):
		ruleNodeLogical(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeLatest, ruleNodeLogical<>> {
	};
	void fProcess() override {
		for (auto parent : lParents) {
			if (lTime < parent->fGetTime()) {
				fSetTime(parent->fGetTime());
				lValue = parent->fGetValueAsBool();
			}
		}
		ruleNode::fProcess();
	}
};




template <unsigned nParents = 0> class ruleNodeArithmetic: public ruleNodeWithParents<nParents> {
  protected:
	double lValue;
  public:
	ruleNodeArithmetic(const std::string& aName, int aNodeId):
		ruleNodeWithParents<nParents>(aName, aNodeId) {
	}
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeArithmetic, ruleNodeWithParents<nParents>> {
	};
	double fGetValueAsDouble() const override {
		return lValue;
	}
	bool fGetValueAsBool() const override {
		return lValue != 0.0;
	};
};

class ruleNodeSum: public ruleNodeArithmetic<> {
  public:
	ruleNodeSum(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeSum, ruleNodeArithmetic<>> {
	};

	void fProcess() override {
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
  public:
	ruleNodeProduct(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeProduct, ruleNodeArithmetic<>> {
	};

	void fProcess() override {
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
  public:
	ruleNodeQuotient(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeQuotient, ruleNodeArithmetic<2>> {
	};
	void fInit() override {
		ruleNodeArithmetic::fInit();
		lDividend = fSetNamedParent("dividend");
		lDivisor = fSetNamedParent("divisor");
	}

	void fProcess() override {
		lValue = lDividend->fGetValueAsDouble() / lDivisor->fGetValueAsDouble();
		fSetTimeFromParents();
		ruleNode::fProcess();
	};
};

class ruleNodeDifference: public ruleNodeArithmetic<2> {
  protected:
	ruleNode* lSubtrahend;
	ruleNode* lMinuend;
  public:
	ruleNodeDifference(const std::string& aName, int aNodeId):
		ruleNodeArithmetic(aName, aNodeId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDifference, ruleNodeArithmetic<2>> {
	};
	void fInit() override {
		ruleNodeArithmetic::fInit();
		lSubtrahend = fSetNamedParent("subtrahend");
		lMinuend = fSetNamedParent("minuend");
	}

	void fProcess() override {
		lValue = lSubtrahend->fGetValueAsDouble() - lMinuend->fGetValueAsDouble();
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
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDelay, ruleNodeArithmetic<1>> {
	};

	double fGetValueAsDouble() const override {
		return lValue;
	}
	bool fGetValueAsBool() const override {
		return lValue != 0.0;
	}
	void fProcess() override {
		std::cout << " processing undelayed " << lName << "\n";
		auto then = lParent->fGetTime() + lDelay.fGetValue();
		lValue = lParent->fGetValueAsDouble();
		fResumeAt(then);
	}
	void fProcessTimed() override {
		ruleNode::fProcess();
	}
};

class ruleNodeConstant: public ruleNode {
  protected:
	slowcontrol::configValue<double> lValue;
  public:
	ruleNodeConstant(const std::string& aName, int aNodeId):
		ruleNode(aName, aNodeId),
		lValue("value", lConfigValues, 1.0) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeConstant, ruleNode> {
	};

	double fGetValueAsDouble() const override {
		return lValue;
	}
	bool fGetValueAsBool() const override {
		return lValue != 0.0;
	}
	void fProcess() override {
	}
};

class ruleNodeMeasurement: public ruleNode {
  protected:
	std::string lQuery;
  public:
	ruleNodeMeasurement(const std::string& aName, int aNodeId) :
		ruleNode(aName, aNodeId) {
	};
	virtual void fSetFromString(const char* aString) = 0;
	virtual const char *fGetValueExpression() = 0;
	void setUpQuery(const std::string& table, slowcontrol::base::uidType uid) {
		lQuery = "SELECT EXTRACT('epoch' from time AT TIME ZONE 'UTC') AS time, ";
		lQuery += fGetValueExpression();
		lQuery += " FROM ";
		lQuery += table;
		lQuery += " WHERE uid = ";
		lQuery += std::to_string(uid);
		lQuery += " ORDER BY time DESC LIMIT 1;";
	}
	void getFromDb() {
		pgsql::request result(lQuery);
		fSetFromString(result.getValue(0, "value"));
		slowcontrol::measurementBase::timeType time(std::chrono::duration_cast<slowcontrol::measurementBase::timeType::duration>(std::chrono::duration<double>(std::stod(result.getValue(0, "time")))));
		fSetTime(time);
	}
};

template <typename T> class ruleNodeTypedMeasurement: public ruleNodeMeasurement {
  protected:
	std::atomic<T> lCurrentValue;
  public:
	ruleNodeTypedMeasurement(const std::string& aName, int aId) :
		ruleNodeMeasurement(aName, aId) {
	}
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeTypedMeasurement, ruleNodeMeasurement> {
	};
	double fGetValueAsDouble() const override {
		return lCurrentValue.load();
	};
	bool fGetValueAsBool() const override {
		return lCurrentValue.load() != 0;
	};
	void fSetFromString(const char* aString) override {
		std::stringstream buf(aString);
		T value;
		buf >> value;
		lCurrentValue = value;
	};
	const char *fGetValueExpression() override {
		if (typeid(bool) == typeid(T)) {
			return " CAST(value AS INTEGER) AS value ";
		}
		return " value ";
		;
	}
};

class ruleNodeFloatMeasurement: public ruleNodeTypedMeasurement<float> {
  public:
	ruleNodeFloatMeasurement(const std::string& aName, int aId) :
		ruleNodeTypedMeasurement(aName, aId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeFloatMeasurement, ruleNodeTypedMeasurement<float>> {
	};
};
class ruleNodeBoolMeasurement: public ruleNodeTypedMeasurement<bool> {
  public:
	ruleNodeBoolMeasurement(const std::string& aName, int aId) :
		ruleNodeTypedMeasurement(aName, aId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeBoolMeasurement, ruleNodeTypedMeasurement<bool>> {
	};
};

class ruleNodeTriggerMeasurement: public ruleNodeMeasurement {
  public:
	ruleNodeTriggerMeasurement(const std::string& aName, int aId) :
		ruleNodeMeasurement(aName, aId) {
	}
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeTriggerMeasurement, ruleNodeMeasurement> {
	};
	double fGetValueAsDouble() const override {
		return 1.0;
	};
	bool fGetValueAsBool() const override {
		return true;
	};
	void fSetFromString(const char* /*aString*/) override {
	};
	const char *fGetValueExpression() override {
		return " 1 AS value ";
	};
};


class ruleNodeAction: public ruleNodeWithParents<1> {
  protected:
	slowcontrol::base::uidType lUid;
  public:
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeAction, ruleNodeWithParents<>> {
	};
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
	double fGetValueAsDouble() const override {
		return lParent->fGetValueAsDouble();
	};
	bool fGetValueAsBool() const override {
		return lParent->fGetValueAsBool();
	};
	void fProcess() override {
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(lUid);
		query += ",";
		std::string request("set ");
		request += std::to_string(static_cast<T>(fGetValueAsDouble()));
		pgsql::fAddEscapedStringToQuery(request, query);
		query += ",'by rule processor');";
		pgsql::request{query};
		ruleNodeAction::fProcess();
	};
};
class ruleNodeFloatAction: public ruleNodeTypedAction<float> {
  public:
	ruleNodeFloatAction(const std::string& aName, int aId) :
		ruleNodeTypedAction(aName, aId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeFloatAction, ruleNodeTypedAction> {
	};
};
class ruleNodeBoolAction: public ruleNodeTypedAction<bool> {
  public:
	ruleNodeBoolAction(const std::string& aName, int aId) :
		ruleNodeTypedAction(aName, aId) {
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeBoolAction, ruleNodeTypedAction> {
	};
};
class ruleNodeTriggerAction: public ruleNodeAction {
  public:
	ruleNodeTriggerAction(const std::string& aName, int aId) :
		ruleNodeAction(aName, aId) {
	}
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeTriggerAction, ruleNodeAction> {
	};
	double fGetValueAsDouble() const override {
		return lParent->fGetValueAsDouble();
	};
	bool fGetValueAsBool() const override {
		return lParent->fGetValueAsBool();
	};
	void fProcess() override {
		std::string query("INSERT INTO setvalue_requests (uid,request,comment) VALUES (");
		query += std::to_string(lUid);
		query += ",'set','by rule processor');";
		pgsql::request{query};
		ruleNodeAction::fProcess();
	};
};



template <typename T> class ruleNodeDerivedValue: public ruleNodeWithParents<1>,
	public slowcontrol::boundCheckerInterface<slowcontrol::measurement<T>> {
  public:
	ruleNodeDerivedValue(const std::string& aName, int aId) :
		ruleNodeWithParents<1>(aName, aId),
		slowcontrol::boundCheckerInterface<slowcontrol::measurement<T>>(std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()) {
	};
	// class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDerivedValue, ruleNodeWithParents<1>> {};
	void fProcess() override {
		this->fStore(lParent->fGetValueAsDouble(), lParent->fGetTime());
		ruleNodeWithParents::fProcess();
	};
	void fInit() override {
		ruleNodeWithParents::fInit();
		this->fInitializeUid(lName);
		this->fConfigure();
	};
};

template <> class ruleNodeDerivedValue<bool>: public ruleNodeWithParents<1>,
	public slowcontrol::measurement<bool> {
  public:
	ruleNodeDerivedValue(const std::string& aName, int aId) :
		ruleNodeWithParents<1>(aName, aId),
		slowcontrol::measurement<bool>() {
	};
	//	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDerivedValue, ruleNodeWithParents<1>> {};
	void fProcess() override {
		fStore(lParent->fGetValueAsBool(), lParent->fGetTime());
		ruleNodeWithParents::fProcess();
	};
	void fInit() override {
		ruleNodeWithParents::fInit();
		fInitializeUid(lName);
		fConfigure();
	};
};

class ruleNodeDerivedFloat : public ruleNodeDerivedValue<float> {
  public:
	ruleNodeDerivedFloat(const std::string& aName, int aId) :
		ruleNodeDerivedValue(aName, aId) {
		lClassName.fSetFromString(__func__);
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDerivedFloat, ruleNodeDerivedValue<float>> {
	};
	double fGetValueAsDouble() const override {
		return const_cast<ruleNodeDerivedFloat*>(this)->fGetCurrentValue();
	};
	bool fGetValueAsBool() const override {
		return const_cast<ruleNodeDerivedFloat*>(this)->fGetCurrentValue() != 0.0;
	};
};
class ruleNodeDerivedBool : public ruleNodeDerivedValue<bool> {
  public:
	ruleNodeDerivedBool(const std::string& aName, int aId) :
		ruleNodeDerivedValue(aName, aId) {
		lClassName.fSetFromString(__func__);
	};
	class ruleNodeCreator: public ruleNode::ruleNodeCreatorTemplate<ruleNodeDerivedBool, ruleNodeDerivedValue<bool>> {
	};
	double fGetValueAsDouble() const override {
		return const_cast<ruleNodeDerivedBool*>(this)->fGetCurrentValue() ? 1.0 : 0.0;
	};
	bool fGetValueAsBool() const override {
		return const_cast<ruleNodeDerivedBool*>(this)->fGetCurrentValue();
	};
};






int main(int argc, const char *argv[]) {
	options::parser parser("slowcontrol program for processing rules");
	parser.fParse(argc, argv);

	ruleNode::fRegisterNodeTypeCreator<ruleNodeOr>("or");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeAnd>("and");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeOddNTrue>("odd");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeEvenNTrue>("even");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeLatch>("latch");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeGreater>("greater");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeEqual>("equal");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeLatest>("latest");


	ruleNode::fRegisterNodeTypeCreator<ruleNodeSum>("sum");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeProduct>("product");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeQuotient>("quotient");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeDifference>("difference");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeDelay>("delay");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeConstant>("constant");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeFloatMeasurement>("measurements_float");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeBoolMeasurement>("measurements_bool");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeTriggerMeasurement>("measurements_trigger");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeFloatAction>("actions_float");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeBoolAction>("actions_bool");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeTriggerAction>("actions_trigger");

	ruleNode::fRegisterNodeTypeCreator<ruleNodeDerivedFloat>("derived_float");
	ruleNode::fRegisterNodeTypeCreator<ruleNodeDerivedBool>("derived_bool");




	auto daemon = new slowcontrol::daemon("ruleProcessord");


	std::map<slowcontrol::base::uidType, ruleNodeMeasurement*> measurements;
	{
		pgsql::request result("SELECT nodetype, nodename,nodeid FROM rule_nodes;");
		for (unsigned int i = 0; i < result.size(); ++i) {
			std::string type(result.getValue(i, "nodetype"));
			std::string name(result.getValue(i, "nodename"));
			auto id = std::stoi(result.getValue(i, "nodeid"));
			bool isMeasurement = type.compare("measurement") == 0;
			bool isAction = type.compare("action") == 0;
			if (isMeasurement || isAction) { // special treatment for measurements

				std::string query("SELECT uid, data_table, is_write_value FROM uid_list WHERE description = ");
				pgsql::fAddEscapedStringToQuery(name, query);
				query += ";";
				pgsql::request res2(query);
				if (res2.size() == 1u) {
					auto table = res2.getValue(0, "data_table");
					auto uid = std::stoi(res2.getValue(0, "uid"));
					if (isMeasurement) {
						auto node = ruleNode::fCreateNode(table, name, id);
						auto measurement = dynamic_cast<ruleNodeMeasurement*>(node);
						measurement->setUpQuery(table, uid);
						measurements.emplace(uid, measurement);
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
	}

	ruleNode::fInitAll();

	pgsql::request{"LISTEN ruleProcessor_measurements;"};




	timedActionsClass& timedActions(*timedActionsClass::fGetInstance());

	daemon->fStartThreads();

	while (!daemon->fGetStopRequested()) {
		auto nextTimeinMs = timedActions.fProcess();
		nextTimeinMs = std::min(nextTimeinMs, 1000);
		nextTimeinMs = std::max(nextTimeinMs, 10);
		struct pollfd pfd;
		pfd.fd = pgsql::getFd();
		pfd.events = POLLIN | POLLPRI;
		if (poll(&pfd, 1, nextTimeinMs) == 0) {
			continue;
		}
		if (pfd.revents & (POLLIN | POLLPRI)) {
			pgsql::consumeInput();
			while (auto notification = pgsql::notification::get()) {
				std::cout << "got notification '" << notification->channel() << "' for " << notification->payload() << std::endl;
				auto uid = std::stoi(notification->payload());
				std::string table(notification->channel() + strlen("ruleProcessor_"));
				auto it = measurements.find(uid);
				if (it != measurements.end()) {
					std::cout << "checking table '" << table << " with " << uid << "'\n";
					it->second->getFromDb();
					it->second->fProcess();
				}
			}
		}
	}


	daemon->fWaitForThreads();
}
