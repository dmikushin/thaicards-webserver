#include "job.h"

#include <sstream>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

void* Job::worker(void* user)
{
	if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL) != 0)
		exit(EXIT_FAILURE);
		
	Job* job = reinterpret_cast<Job*>(user);
	if (!job) exit(EXIT_FAILURE);

	// Dump JSON to a temporary file.
	char filename[] = "/tmp/thaicards.XXXXXX";
	int fd = mkstemp(filename);
	if (fd == -1) exit(EXIT_FAILURE);
	fchmod(fd, 0666);

	Json::StyledWriter styledWriter;
	string str = styledWriter.write(job->json);
	const char* cstr = str.c_str();
	write(fd, cstr, strlen(cstr));
	close(fd);

	// Execute thaicards generator.
	stringstream ss;
	ss << "mkdir -p /tmp/";
	ss << job->timestamp;
	ss << " && chmod 0777 /tmp/";
	ss << job->timestamp;
	ss << " && cd /tmp/";
	ss << job->timestamp;
	ss << " && cp ";
	ss << filename;
	ss << " . && thaicards ";
	ss << filename;
	string cmd = ss.str();
	system(cmd.c_str());

	job->ready = true;

	unlink(filename);
		
	return NULL;
}

Job::Job() : ready(false) { }
	
void Job::execute(const uint64_t timestamp_, const Json::Value& json_)
{
	timestamp = timestamp_;
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

