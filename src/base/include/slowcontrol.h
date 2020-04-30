#ifndef __slowcontrol_h_
#define __slowcontrol_h_


#include <thread>
#include <map>
#include <unistd.h>

namespace slowcontrol {

	class base {
	  public:
		typedef int32_t uidType;
	  private:
		static std::string gHostname;
	  public:
		static const std::string& fGetHostName();
		static int fGetCompoundId(const char* aName, const char *aDescription = nullptr);
		static int fSelectOrInsert(const char *aTable, const char *aIdColumn,
		                           const char *aKeyColumn, const char *aKeyValue,
		                           const char *aExtraColum = nullptr, const char *aExtraValue = nullptr,
		                           bool *aInsertWasDone = nullptr);
		static void fAddToCompound(int aCompound, uidType aUid, const char* aName);
		static void fAddSubCompound(int aParent, int aChild, const char* aName);
		static void fAddToCompound(int aCompound, uidType aUid, const std::string& aName);
		static void fAddSubCompound(int aParent, int aChild, const std::string& aName);
		static bool fRequestValueSetting(uidType aUid, const std::string& aRequest,
		                                 const std::string& aComment,
		                                 std::string& aResponse);
	};

	class exception: public std::exception {
	  public:
		enum class level {
			kNone,
			kContinue,
			kStop
		};
	  protected:
		std::string lWhat;
		level lLevel;
	  public:
		exception(const char *aWhat, level aLevel) :
			lWhat(aWhat),
			lLevel(aLevel) {
		};
		virtual const char* what() const noexcept {
			return lWhat.c_str();
		};
		virtual level fGetLevel() const {
			return lLevel;
		};
	};

} // end of namespace slowcontrol
#endif
