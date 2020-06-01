#include "job.h"

using namespace std;

void* Job::worker(void* user)
{
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
		exit(EXIT_FAILURE);
		
	Job* job = reinterpret_cast<Job*>(user);
	if (!job) exit(EXIT_FAILURE);

	// TODO Dump JSON to a temporary file.

	// TODO Execute thaicards generator.
		
	job->ready = true;
		
	return NULL;
}

Job::Job() : ready(false) { }
	
void Job::execute(const Json::Value& json_)
{
	json = json_;
	
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

	return "";
}

Job::~Job()
{
	pthread_cancel(thread);
	pthread_join(thread, NULL);
}

