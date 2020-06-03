#include "job.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string.h>

#ifdef __APPLE__
#include <ghc/filesystem.hpp>
namespace fs = ghc::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

using namespace std;

void* Job::worker(void* user)
{
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
		exit(EXIT_FAILURE);
		
	Job* job = reinterpret_cast<Job*>(user);
	if (!job) exit(EXIT_FAILURE);

	// Create job results folder.
	{
	        stringstream ss;
        	ss << "mkdir -p /tmp/" BACKEND "/";
	        ss << job->timestamp;
		string cmd = ss.str();
		system(cmd.c_str());
	}

	// Write job JSON to file.
	string filename;
	{
		stringstream ss;
		ss << "/tmp/" BACKEND "/";
		ss << job->timestamp;
		ss << "/" BACKEND ".json";
		filename = ss.str();

		Json::StyledWriter styledWriter;
		string str = styledWriter.write(job->json);
		std::ofstream file(filename);
		file << str;
		file.close();
	}

	// Execute cards generator.
	{
		stringstream ss;
		ss << "cd /tmp/" BACKEND "/";
		ss << job->timestamp;
		ss << " && " BACKEND " ";
		ss << filename;
		string cmd = ss.str();
		system(cmd.c_str());
	}

	job->ready = true;

	return NULL;
}

Job::Job() : ready(false) { }

void Job::execute(const uint64_t timestamp_, const Json::Value& json_)
{
	timestamp = timestamp_;
	json = json_;
	ncards = json["cards"].size();
	
	pthread_create(&thread, NULL, &Job::worker, this);
}
	
string Job::getResult()
{
	if (!ready)
	{
		return
			"<html>"
			"Results are not yet ready. Please try again later."
			"</html>";
	}

	stringstream ss;
	ss << "<html>";
	for (int i = 0; i < ncards; i++)
	{
		ss << "<img src=\"/request/";
		ss << timestamp;
		ss << "/" BACKEND "_";
		ss << setfill('0') << setw(3) << (i + 1);
		ss << ".png\" /><br />";
	}
	ss << "</html>";

	return ss.str();
}

Job::~Job()
{
	if (!ready)
	{
		pthread_cancel(thread);
		pthread_join(thread, NULL);
	}
}

void JobServer::submit(const uint64_t timestamp, const Json::Value& json)
{
	jobs[timestamp].execute(timestamp, json);
}

bool JobServer::getResult(const uint64_t timestamp, string& result)
{
	map<uint64_t, Job>::iterator it = jobs.find(timestamp);
	if (it == jobs.end())
		return false;

	result = it->second.getResult();
	return true;
}

JobServer::JobServer()
{
	string path = "/tmp/" BACKEND "/";
	if (!fs::exists(path))
		return;
	if (!fs::is_directory(path))
		return;

	// Import existing results.
	for (const auto& entry : fs::directory_iterator(path))
	{
		if (!fs::is_directory(entry))
			continue;

		const fs::path& path = entry.path();

		// Parse the timestamp.
		const string stimestamp = path.filename();
		uint64_t timestamp;
		if (!(stringstream(stimestamp) >> timestamp))
			continue;

		fs::path jsonPath = path / BACKEND ".json";
		if (!fs::exists(jsonPath))
			continue;
		if (!fs::is_regular_file(jsonPath))
			continue;

		// Read the JSON file.
		const string filename = jsonPath.string();
		ifstream file(filename.c_str());
		if (!file.is_open())
			continue;

		stringstream buffer;
		buffer << file.rdbuf();
		string json = buffer.str();

		// Ensure JSON is valid and count the cards.
		size_t ncards = 0;
		try
		{
			Json::Value json_parsed;
			json_parsed.clear();
			Json::Reader reader;
			if (!reader.parse(json, json_parsed))
				continue;

			ncards = json_parsed["cards"].size();
		}
		catch (exception &e)
		{
			continue;
		}

		// Create the job and add it to the job server.
		Job& job = jobs[timestamp];
		job.timestamp = timestamp;
		job.json = json;
		job.ncards = ncards;
		job.ready = true;
	}
}

