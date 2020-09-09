#include <Options.h>
#include <curl/curl.h>
#include <string>
#include <iostream>








int main(int argc, const char *argv[]) {
	options::parser parser("program to issue a post  reqest on a http srever");
	options::single<std::string> url('u', "url", "url to post request to");
	options::map<std::string> items('i', "item", "item map");
	parser.fParse(argc, argv);

	auto *curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	auto* mime = curl_mime_init(curl);
	for (const auto& item : items) {
		auto *part = curl_mime_addpart(mime);
		curl_mime_data(part, item.second.c_str(), CURL_ZERO_TERMINATED);
		curl_mime_name(part, item.first.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
	char curlErrorBuffer[CURL_ERROR_SIZE];
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuffer);
	auto result = curl_easy_perform(curl);
	if (result != CURLE_OK) {
		std::cerr << curlErrorBuffer;
	}
	curl_easy_cleanup(curl);
	return 0;
}
