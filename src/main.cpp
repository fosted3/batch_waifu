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

bool dir_exists(const char *path)
{
	DIR *dir = opendir(path);
	if (dir)
	{
		closedir(dir);
		return true;
	}
	else if (errno == ENOENT)
	{
		return false;
	}
	else
	{
		std::cerr << std::strerror(errno) << std::endl;
		exit(errno);
	}
}

void create_dir_tree(const char *base_path, const char *data_path)
{
	std::string ptemp = base_path;
	std::vector<std::string> to_create = {"/images", "/out", "/images/original", "/images/upscaled"};
	ptemp += "/";
	ptemp += data_path;
	if (mkdir(ptemp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
	{
		if (errno != EEXIST)
		{
			std::cerr << std::strerror(errno) << std::endl;
			exit(errno);
		}
	}
	for (auto itr = to_create.begin(); itr != to_create.end(); itr++)
	{
		ptemp = base_path;
		ptemp += "/";
		ptemp += data_path;
		ptemp += *itr;
		if (mkdir(ptemp.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			if (errno != EEXIST)
			{
				std::cerr << std::strerror(errno) << std::endl;
				exit(errno);
			}
		}
	}
}

void make_work(std::vector<work_unit_t *> *work, DIR *dir, const char *base_path)
{
	dirent *dp = NULL;
	struct stat st;
	std::vector<std::string> dirs;
	std::vector<std::string> files;
	bool match_dir;
	std::string dtemp;
	std::string ftemp;
	std::string ptemp;
	std::string data_dir; 
	std::string original_image_dir;
	std::string upscaled_image_dir;
	std::string output_dir;
	DIR *dptemp;
	while (dir)
	{
		dp = readdir(dir);
		if (dp != NULL)
		{
			ptemp = base_path;
			ptemp += "/";
			ptemp += dp -> d_name;
			if (lstat(ptemp.c_str(), &st) == 0)
			{
				if (S_ISDIR(st.st_mode))
				{
					dirs.push_back(std::string(dp -> d_name));
				}
				else
				{
					files.push_back(std::string(dp -> d_name));
				}
			}
			else
			{
				std::cerr << std::strerror(errno) << std::endl;
				closedir(dir);
				exit(errno);
			}
		}
		else
		{
			closedir(dir); //ensure that this gets called - errors from lstat or readdir may cause this to fail
			break;
		}
	}
	//if (dp) { delete dp; }
	for (auto fitr = files.begin(); fitr != files.end(); fitr++)
	{
		if (valid(*fitr))
		{
			create_dir_tree(base_path, fitr -> substr(0, fitr -> find_last_of(".")).c_str());
			data_dir = base_path;
			data_dir += "/";
			data_dir += fitr -> substr(0, fitr -> find_last_of("."));
			original_image_dir = data_dir;
			original_image_dir += "/images/original";
			upscaled_image_dir = data_dir;
			upscaled_image_dir += "/images/upscaled";
			output_dir = data_dir;
			output_dir += "/out";
			//check for existing extracted images
			dptemp = opendir(original_image_dir.c_str());
			while (dptemp)
			{
				dp = readdir(dptemp);
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
			dptemp = opendir(ptemp.c_str());
			make_work(work, dptemp, ptemp.c_str());
		}
	}
	//if (dp) { delete dp; }
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
	if (argc > 1)
	{
		strcpy(buf, argv[1]);
	}
	else if (getcwd(buf, buf_size) == NULL)
	{
		std::cerr << std::strerror(errno) << std::endl;
		return errno;
	}
	DIR *dir = opendir(buf);
	make_work(&work, dir, buf);
	do_work(&work, &vector_lock);
	delete[] buf;
	return 0;
}
