#ifndef __pgsqlWrapper_h__
#define __pgsqlWrapper_h__
#include <string>
#include <memory>
class pg_result;
class pgNotify;

namespace pgsql {

	class request {
	  protected:
		pg_result* result;
	  public:
		request() = delete;
		explicit request(const std::string& command);
		~request();
		void update(const std::string& command);
		const char* getValue(int row, int column);
		const char* getValue(int row, const std::string& column);
		size_t size() const;
	};
	int getFd();
	void consumeInput();
	std::unique_ptr<pgNotify, void(*)(void*)> getNotifcation();
	void fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery);

}; // end of pqsql namespace


#endif
