#include "wait.h"

#include <map>
#include <memory>
#include <sstream>
#include <vector>

using namespace std;

// Container for embedded content that shall be
// loaded and persist in memory during the server lifetime.
extern unique_ptr<map<string, vector<unsigned char>*> > www_data;

const string Wait::getHtml(const uint64_t timestamp)
{printf("0");
	if (!www_data) return ""; printf("1");

	const vector<unsigned char>* content = (*www_data)["wait.html.in"];
	if (!content) return ""; printf("2");

	string html = string(reinterpret_cast<const char*>(&(*content)[0]),
		content->size());

	// TODO Use regex
	stringstream ss;
	ss << timestamp;	     
	string stimestamp = ss.str();
	string placeholder = "__TIMESTAMP__";
	while (1)
	{       
		auto found = html.find(placeholder);
		if (found == string::npos)
			break;
				
		html.replace(found, placeholder.length(), stimestamp);
	}
	
	return html;
}

Wait::Wait() { }

