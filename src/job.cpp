#include "job.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string.h>

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
	pthread_cancel(thread);
	pthread_join(thread, NULL);
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
	// TODO Import existing results.
}

