#ifndef __slowcontrol_h_
#define __slowcontrol_h_


#include <thread>
#include <map>
#include <pgsql/libpq-fe.h>
#include <unistd.h>

class slowcontrol {
  private:
	static std::map<std::thread::id, PGconn *> gConnections;
	static std::string gHostname;
  public:
	static PGconn *fGetDbconn();
	static const std::string& fGetHostName();
	static int fGetCompoundId(const char* aName, const char *aDescription = nullptr);
	static int fSelectOrInsert(const char *aTable, const char *aIdColumn,
	                           const char *aKeyColumn, const char *aKeyValue,
	                           const char *aExtraColum = nullptr, const char *aExtraValue = nullptr,
	                           bool *aInsertWasDone = nullptr);
	static void fAddToCompound(int aCompound, int aUid, const char* aName);
	static void fAddSubCompound(int aParent, int aChild, const char* aName);
	static void fAddToCompound(int aCompound, int aUid, const std::string& aName);
	static void fAddSubCompound(int aParent, int aChild, const std::string& aName);
	static void fAddEscapedStringToQuery(const char *aString, std::string& aQuery);
	static void fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery);
};

#endif
