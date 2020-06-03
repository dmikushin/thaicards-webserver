#ifndef JOB_H
#define JOB_H

#include <atomic>
#include <cstdlib>
#include <map>
#include <json/json.h>
#include <pthread.h>
#include <string>

class JobServer;

class Job
{
	uint64_t timestamp;
	Json::Value json;
	size_t ncards;

	pthread_t thread;
	std::atomic<bool> ready;
	
	static void* worker(void* user);

public :

	Job();

	Job(const uint64_t timestamp, const Json::Value& json);
	
	void execute(const uint64_t timestamp, const Json::Value& json);
	
	std::string getResult();

	~Job();

	friend class JobServer;
};

class JobServer
{
	std::map<uint64_t, Job> jobs;

public :

	void submit(const uint64_t timestamp, const Json::Value& json);

	bool getResult(const uint64_t timestamp, std::string& result);

	JobServer();
};

#endif // JOB_H

