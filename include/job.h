#ifndef JOB_H
#define JOB_H

#include <atomic>
#include <cstdlib>
#include <json/json.h>
#include <pthread.h>
#include <string>

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
	
	void execute(const uint64_t timestamp, const Json::Value& json);
	
	std::string getResult();

	~Job();
};

#endif // JOB_H

