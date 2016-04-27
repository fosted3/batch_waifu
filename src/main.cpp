#include <string>
#include <mutex>
#include <queue>
#include <vector>
#include <cstdint>
#include <iostream>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>

typedef enum
{
	extract_image,
	extract_audio,
	upscale_image,
	render_video
} work_t;

typedef struct
{
	uint64_t id;
	std::string working_dir;
	std::string command;
	std::mutex work_lock;
	std::queue<uint64_t> deps;
	work_t type;
	bool done;
} work_unit_t;

void create_thread(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
	int rc = pthread_create(thread, attr, start_routine, arg);
	if (rc)
	{
		std::cerr << "Could not create thread." << std::endl;
		exit(1);
	}
}

bool valid(const std::string &file)
{
	std::vector<std::string> extensions = {"mp4", "mkv", "m4v", "avi"};
	for (auto itr = extensions.begin(); itr != extensions.end(); itr++)
	{
		if (file.size() >= itr -> size() && file.compare(file.size() - itr -> size(), itr -> size(), *itr) == 0) { return true; }
	}
	return false;
}

void make_work(std::vector<work_unit_t *> *work, DIR *dir, const char *base_path)
{
	//std::cout << base_path << std::endl;
	//std::cout << dir << std::endl;
	dirent *dp = NULL;
	struct stat st;
	std::vector<std::string> dirs;
	std::vector<std::string> files;
	bool match_dir;
	std::string dtemp;
	std::string ftemp;
	std::string ptemp;
	while (dir)
	{
		//end up calling do_work with *NEW* dir ptr
		dp = readdir(dir);
		if (dp != NULL)
		{
			//std::cout << dp -> d_name << std::endl;
			ptemp = base_path;
			ptemp += "/";
			ptemp += dp -> d_name;
			if (lstat(ptemp.c_str(), &st) == 0)
			{
				//std::cout << "no error" << std::endl;
				//std::cout << "pushing back " << dp -> d_name << std::endl;
				if (S_ISDIR(st.st_mode))
				{
					dirs.push_back(std::string((const char*) dp -> d_name));
				}
				else
				{
					files.push_back(std::string((const char*) dp -> d_name));
				}
			}
			else
			{
				//std::cout << lstat(dp -> d_name, &st) << std::endl;
				std::cerr << std::strerror(errno) << std::endl;
				exit(errno);
			}
		}
		else
		{
			closedir(dir);
			break;
		}
	}
	if (dp) { delete dp; }
	for (auto fitr = files.begin(); fitr != files.end(); fitr++)
	{
		if (valid(*fitr))
		{
			//std::cout << *fitr << " is valid" << std::endl;
			match_dir = false;
			for (auto ditr = dirs.begin(); ditr != dirs.end(); ditr++)
			{
				//std::cout << *ditr << std::endl;
				if (ditr -> size() < fitr -> size() && ditr -> compare(0, std::string::npos, fitr -> c_str(), ditr -> size()) == 0)
				{
					match_dir = true;
					std::cout << *ditr << " matches " << *fitr << "!" << std::endl;
					dtemp = *ditr;
					ftemp = *fitr;
					break;
				}
			}
			if (!match_dir)
			{
				ptemp = base_path;
				ptemp += "/";
				ptemp += fitr -> substr(0, fitr -> find_last_of("."));
				//std::cout << ptemp << std::endl;
				if (mkdir(ptemp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
				{
					std::cerr << std::strerror(errno) << std::endl;
					exit(errno);
				}
			}
		}
	}
	/* a miracle happens */
	for (auto ditr = dirs.begin(); ditr != dirs.end(); ditr++)
	{
		if (ditr -> compare(".") && ditr -> compare(".."))
		{
			ptemp = base_path;
			ptemp += "/";
			ptemp += *ditr;
			DIR *newdir = opendir(ptemp.c_str());
			make_work(work, newdir, ptemp.c_str());
			
		}
	}
}

void do_work(std::vector<work_unit_t *> *work, std::mutex *vector_lock)
{
	
}

int main(int argc, char** argv)
{
	std::vector<work_unit_t*> work;
	std::mutex vector_lock;
	size_t buf_size = 1024;
	char* buf = new char[buf_size];
	if (getcwd(buf, buf_size) == NULL)
	{
		return -1;
	}
	//std::cout << buf << std::endl;
	DIR *dir = opendir(buf);
	make_work(&work, dir, buf);
	delete[] buf;
	return 0;
}
