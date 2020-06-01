#include "index.h"
#include "job.h"
#include "sslkeys.h"
#include "timing.h"
#include "wait.h"

#include <cmath>
#include <climits> // HOST_NAME_MAX
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <json/json.h>
#include <map>
#include <memory>
#include <microhttpd.h>
#include <regex>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <vector>

#define DATASETSIZE (1024 * 1024)
#define POSTBUFFERSIZE 65536

using namespace std;

// Container for embedded content that shall be
// loaded and persist in memory during the server lifetime.
unique_ptr<map<string, vector<unsigned char>*> > www_data;

// Container for embedded content MIME.
unique_ptr<map<string, string> > www_data_mime;

static Index indexPage;
static Wait waitPage;

map<uint64_t, Job> jobs;

class Post
{
	const char* empty;
	vector<char> json;

public :

	Post() : empty("") { }

	const char* getJson()
	{
		if (json.size())
			return (const char*)&json[0];
		
		return empty;
	}

	MHD_PostProcessor* processor;

	static int process(void* con_cls, enum MHD_ValueKind kind, const char* key,
		const char* filename, const char* content_type, const char* transfer_encoding,
		const char* data, uint64_t offset, size_t size)
	{
		if (!strcmp(key, "json") && size)
		{
			Post* post = reinterpret_cast<Post*>(con_cls);
		
			if (post->json.size() < offset + size)
			{
				if (offset + size > DATASETSIZE)
					return MHD_NO;

				post->json.resize(offset + size + 1);
			}
		
			memcpy(&post->json[offset], data, size);
			post->json[post->json.size() - 1] = '\0';
		}

		return MHD_YES;
	}

	static void finalize(void* cls, struct MHD_Connection* connection,
		void** con_cls, enum MHD_RequestTerminationCode toe)
	{
		Post* post = reinterpret_cast<Post*>(*con_cls);

		if (!post) return;
		
		MHD_destroy_post_processor(post->processor);
		delete post;
		*con_cls = NULL;
	}
};

static int result_404(struct MHD_Connection* connection)
{	
	static struct MHD_Response* response = NULL;
	
	if (!response)
	{
		static string message_404 = "<html><body><h2>404 Not found</h2></body></html>";
		response = MHD_create_response_from_buffer(
			message_404.size(), &message_404[0], MHD_RESPMEM_PERSISTENT);
	}

	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);

	return ret;
}

static bool getFile(const char* filename, string& result, string& mime)
{
	if (!www_data) return false;

	const vector<unsigned char>* content = (*www_data)[filename];
	if (!content) return false;
	
	result = string(reinterpret_cast<const char*>(&(*content)[0]), content->size());

	if (!www_data_mime) return true;

	mime = (*www_data_mime)[filename];

	return true;
}

static int callback(void* cls, struct MHD_Connection* connection,
	const char* curl, const char* method, const char* version,
	const char* upload_data, size_t* upload_data_size, void** con_cls)
{
	if (!strcmp(method, "POST"))
	{
		if (!*con_cls)
		{
			Post* post = new Post();
		
			if (!post)
				return MHD_NO;

			post->processor = MHD_create_post_processor(
				connection, POSTBUFFERSIZE, Post::process, (void*)post);

			if (!post->processor)
			{
				delete post;
				return MHD_NO;
			}

			*con_cls = (void*)post;
			
			return MHD_YES;
		}

		Post* post = reinterpret_cast<Post*>(*con_cls);

		if (*upload_data_size != 0)
		{
			MHD_post_process(post->processor, upload_data, *upload_data_size);
			*upload_data_size = 0;
			
			return MHD_YES;
		}
	}

	string result = "";
	string mime = "text/html";

	if (!strcmp(curl, "/"))
	{
		result = indexPage.getHtml();
	}
	else if (!strcmp(curl, "/process"))
	{
		Post* post = reinterpret_cast<Post*>(*con_cls);
		if (!post)
		{
			result =
				"<html>"
				"<h1>Bad Request</h1>"
				"</html>";
		}
		else
		{
			string content(post->getJson());

			// Strip lines starting with #.
			regex r("#.*\r?\n");
			std::string json = content;
			while (std::regex_search(json, r))
				json = std::regex_replace(json, r, "");

			// Ensure string parses as correct JSON.
			Json::Value json_parsed;
			try
			{
				json_parsed.clear();
				Json::Reader reader;
				if (reader.parse(json, json_parsed))
				{
					uint64_t timestamp;
					get_time(&timestamp);

					jobs[timestamp].execute(timestamp, json_parsed);

					result = waitPage.getHtml(timestamp);
				}
				else
				{
					stringstream ss;
					ss << "<html>";
					ss << "<h1>Invalid JSON</h1>";
					ss << reader.getFormatedErrorMessages();
					ss << "</html>";
					result = ss.str();
				}
			}
			catch (exception &e)
			{
				stringstream ss;
				ss << "<html>";
				ss << "<h1>Invalid JSON</h1>";
				ss << e.what();
				ss << "</html>";
				result = ss.str();
			}
		}
	}
	else if (!memcmp(curl, "/request/", sizeof("/request/") - 1))
	{
		string stimestamp(curl + sizeof("/request/") - 1);
		uint64_t timestamp;

		if (!(stringstream(stimestamp) >> timestamp))
			return result_404(connection);
			
		std::size_t found = stimestamp.find_first_of("/");
		if (found != string::npos)
		{
			string simage(curl + sizeof("/request/") - 1 + found);

			// Try to interpret as a card file request.
			stringstream ss;
			ss << "/tmp/";
			ss << timestamp;
			ss << simage;
			string filename = ss.str();
			ifstream file(filename.c_str());

			if (!file.is_open())
				return result_404(connection);

			stringstream buffer;
			buffer << file.rdbuf();
			result = buffer.str();
			mime = "image/png";
		}
		else
		{
			map<uint64_t, Job>::iterator it = jobs.find(timestamp);
			if (it == jobs.end())
				return result_404(connection);

			result = it->second.getResult();
		}
	}
	else if (!getFile(&curl[1], result, mime))
	{
		return result_404(connection);
	}	

	// Reset when done.
	struct MHD_Response* response = MHD_create_response_from_buffer(
		result.size(), &result[0], MHD_RESPMEM_MUST_COPY);
	MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime.c_str());
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
    
	return ret;
}

static int result_302(void* cls, struct MHD_Connection* connection,
	const char* curl, const char* method, const char* version,
	const char* upload_data, size_t* upload_data_size, void** con_cls)
{	
	static struct MHD_Response* response = NULL;
	
	if (!response)
	{
		static string message_302 = "<html><body><h2>302 Found</h2></body></html>";
		response = MHD_create_response_from_buffer(
			message_302.size(), &message_302[0], MHD_RESPMEM_PERSISTENT);
		static char hostname[sizeof("https://") + HOST_NAME_MAX] = "https://";
		gethostname(hostname + sizeof("https://") - 1, HOST_NAME_MAX + 1);
		MHD_add_response_header(response, "Location", hostname);
	}

	int ret = MHD_queue_response(connection, MHD_HTTP_FOUND, response);
	
	return ret;
}

static MHD_Daemon* startHTTPS()
{
	static SSLKeys sslKeys;

	// Use SSL.
	return MHD_start_daemon(
		MHD_USE_POLL_INTERNALLY | MHD_USE_SSL, 443, NULL, NULL,
		&callback, NULL,
		MHD_OPTION_HTTPS_MEM_KEY, sslKeys.getPrivateKey().c_str(),
		MHD_OPTION_HTTPS_MEM_CERT, sslKeys.getCertificate().c_str(),
		MHD_OPTION_NOTIFY_COMPLETED, Post::finalize, NULL,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
		MHD_OPTION_END);
}

static MHD_Daemon* startHTTP(int port, bool redirectHTTPtoHTTPS = false)
{
	MHD_Daemon* daemon = MHD_start_daemon(
		MHD_USE_POLL_INTERNALLY, port, NULL, NULL,
		redirectHTTPtoHTTPS ? &result_302 : &callback, NULL,
		MHD_OPTION_NOTIFY_COMPLETED, Post::finalize, NULL,
		MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)120,
		MHD_OPTION_END);
	
	if (!redirectHTTPtoHTTPS) return daemon;

	if (daemon == NULL)
	{
		fprintf(stderr, "Error starting server on port %d, errno = %d\n", port, errno);
		exit(1);
	}

	while (1) sleep(1);

	MHD_stop_daemon(daemon);
	
	return NULL;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("%s <port>\n", argv[0]);
		return 1;
	}
	
	int port = atoi(argv[1]);

	MHD_Daemon* daemon;
	
	std::thread httpRedirection;
	
	if (port == 443)
	{
		daemon = startHTTPS();

		// Start HTTP as well, but only for redirection.
		bool redirectHTTPtoHTTPS = true;
		httpRedirection = std::thread(startHTTP, 80, true);
	}
	else
		daemon = startHTTP(port);

	if (daemon == NULL)
	{
		fprintf(stderr, "Error starting server on port %d, errno = %d\n", port, errno);
		return 1;
	}

	while (1) sleep(1);

	MHD_stop_daemon(daemon);

	httpRedirection.join();

	return 0;
}

