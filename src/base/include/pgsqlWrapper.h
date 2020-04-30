#ifndef __pgsqlWrapper_h__
#define __pgsqlWrapper_h__
#include <string>
#include <memory>

namespace pgsql {
	class pg_result;
	class pgNotify;

	class request {
	  protected:
		pg_result* result;
	  public:
		request(const std::string& command);
		~request();
		void update(const std::string& command);
		const char* getValue(int row, int column);
		const char* getValue(int row, const std::string& column);
		size_t size() const;
	};
	void consumeInput();
	std::unique_ptr<pgNotify, void(*)(void*)> getNotifcation();
	void fAddEscapedStringToQuery(const std::string& aString, std::string& aQuery);

}; // end of pqsql namespace


#endif
