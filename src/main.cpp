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

/*typedef enum
{
	not_started,
	in_progress,
	done
} status_t;*/

typedef struct
{
	uint64_t id;
	std::string *working_dir;
	std::string *command;
	//std::mutex *work_lock;
	std::queue<uint64_t> *deps;
	work_t type;
	int status;
} work_unit_t;

typedef struct
{
	std::vector<work_unit_t *> *work;
	std::mutex *vector_lock;
} thread_data_t;

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

bool valid(const char *file)
{
	std::string temp = std::string(file);
	return valid(temp);
}

bool isext(const char *file, const char *ext)
{
	size_t file_len = strlen(file);
	size_t ext_len = strlen(ext);
	if (file_len < ext_len) { return false; }
	if (strcmp((const char *) file + file_len - ext_len, ext))
	{
		return false;
	}
	return true;
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
	//bool match_dir;
	std::string dtemp;
	std::string ftemp;
	std::string ptemp;
	std::string data_dir;
	std::string original_image_dir;
	std::string upscaled_image_dir;
	std::string output_dir;
	bool orig_img;
	bool upscale_img;
	bool video;
	bool audio;
	work_unit_t *wtemp;
	uint64_t orig_img_id;
	uint64_t audio_id;
	uint64_t upscale_img_id;
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
			orig_img = false;
			upscale_img = false;
			video = false;
			audio = false;
			//check for existing extracted images
			dptemp = opendir(original_image_dir.c_str());
			while (dptemp)
			{
				dp = readdir(dptemp);
				if (dp != NULL)
				{
					if (isext(dp -> d_name, "png"))
					{
						orig_img = true;
						closedir(dptemp);
						break;
					}
				}
				else
				{
					closedir(dptemp);
					break;
				}
			}
			//check for any upscaled images
			dptemp = opendir(upscaled_image_dir.c_str());
			while (dptemp)
			{
				dp = readdir(dptemp);
				if (dp != NULL)
				{
					if (isext(dp -> d_name, "png"))
					{
						upscale_img = true;
						closedir(dptemp);
						break;
					}
				}
				else
				{
					closedir(dptemp);
					break;
				}
			}
			//check for video
			dptemp = opendir(output_dir.c_str());
			while (dptemp)
			{
				dp = readdir(dptemp);
				if (dp != NULL)
				{
					if (valid(dp -> d_name))
					{
						video = true;
						closedir(dptemp);
						break;
					}
				}
				else
				{
					closedir(dptemp);
					break;
				}
			}
			dptemp = opendir(data_dir.c_str());
			while (dptemp)
			{
				dp = readdir(dptemp);
				if (dp != NULL)
				{
					if (strcmp("audio", dp -> d_name) == 0)
					{
						audio = true;
						closedir(dptemp);
						break;
					}
				}
				else
				{
					closedir(dptemp);
					break;
				}
			}
			if (!orig_img && !video)
			{
				wtemp = new work_unit_t;
				wtemp -> id = work -> size();
				orig_img_id = wtemp -> id;
				wtemp -> working_dir = new std::string(original_image_dir);
				wtemp -> command = new std::string("ffmpeg -i \"../../");
				*(wtemp -> command) += *fitr;
				*(wtemp -> command) += "\" %05d.png";
				//wtemp -> work_lock = new std::mutex;
				wtemp -> deps = new std::queue<uint64_t>;
				wtemp -> type = extract_image;
				wtemp -> status = 0;
				work -> push_back(wtemp);
			}
			if (!upscale_img && !video)
			{
				wtemp = new work_unit_t;
				wtemp -> id = work -> size();
				upscale_img_id = wtemp -> id;
				wtemp -> working_dir = new std::string(original_image_dir);
				wtemp -> command = new std::string("for f in *; do waifu2x -i $f -o ../upscaled/$f; done");
				//wtemp -> work_lock = new std::mutex;
				wtemp -> deps = new std::queue<uint64_t>;
				wtemp -> type = upscale_image;
				wtemp -> status = 0;
				if (!orig_img)
				{
					wtemp -> deps -> push(orig_img_id);
				}
				work -> push_back(wtemp);
			}
			if (!audio && !video)
			{
				wtemp = new work_unit_t;
				wtemp -> id = work -> size();
				audio_id = wtemp -> id;
				wtemp -> working_dir = new std::string(data_dir);
				wtemp -> command = new std::string("ffmpeg -i \"../");
				*(wtemp -> command) += *fitr;
				*(wtemp -> command) += "\" -vn -acodec copy audio.ext";
				//wtemp -> work_lock = new std::mutex;
				wtemp -> deps = new std::queue<uint64_t>;
				wtemp -> type = extract_audio;
				wtemp -> status = 0;
				work -> push_back(wtemp);
			}
			if (!video)
			{
				wtemp = new work_unit_t;
				wtemp -> id = work -> size();
				wtemp -> working_dir = new std::string(output_dir);
				wtemp -> command = new std::string("ffmpeg -framerate 30 -i ../images/upscaled/%05d.png -i ../audio.ext -c:v libx264 -c:a aac -strict experimental -b:a 192k -shortest -pix_fmt yuv420p \"");
				*(wtemp -> command) += *fitr;
				*(wtemp -> command) += "\"";
				//wtemp -> work_lock = new std::mutex;
				wtemp -> deps = new std::queue<uint64_t>;
				wtemp -> type = render_video;
				wtemp -> status = 0;
				if (!orig_img)
				{
					wtemp -> deps -> push(orig_img_id);
				}
				if (!upscale_img)
				{
					wtemp -> deps -> push(upscale_img_id);
				}
				if (!audio)
				{
					wtemp -> deps -> push(audio_id);
				}
				work -> push_back(wtemp);
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

void execute_unit(work_unit_t *unit)
{
	//std::cout << unit -> id << std::endl;
	//std::cout << *(unit -> working_dir) << std::endl;
	
}

void *work_thread(void *data)
{
	thread_data_t *thread_data = (thread_data_t *) data;
	work_unit_t *unit;
	bool done = false;
	do
	{
		done = true;
		unit = NULL;
		thread_data -> vector_lock -> lock();
		for (auto itr = thread_data -> work -> begin(); itr != thread_data -> work -> end(); itr++)
		{
			//(*itr) -> work_lock -> lock();
			if ((*itr) -> status == 0 && (*itr) -> deps -> empty())
			{
				//std::cout << "ID " << (*itr) -> id << " has no deps. Starting." << std::endl;
				unit = *itr;
			}
			else if ((*itr) -> status == 0 && !((*itr) -> deps -> empty()))
			{
				do
				{
					//thread_data -> work -> at((*itr) -> deps -> front()) -> work_lock -> lock();
					if (thread_data -> work -> at((*itr) -> deps -> front()) -> status == 2)
					{
						//thread_data -> work -> at((*itr) -> deps -> front()) -> work_lock -> unlock();
						(*itr) -> deps -> pop();
					}
					else
					{
						//thread_data -> work -> at((*itr) -> deps -> front()) -> work_lock -> unlock();
						break;
					}
				} while (!((*itr) -> deps -> empty()));
				if ((*itr) -> deps -> empty())
				{
					//std::cout << "ID " << (*itr) -> id << " has deps but they're done. Starting." << std::endl;
					unit = *itr;
				}
				/*else
				{
					std::cout << "ID " << (*itr) -> id << " has unfinished deps." << std::endl;
				}*/
			}
			if (unit) { break; }
			//(*itr) -> work_lock -> unlock();
		}
		if (unit)
		{
			//unit -> work_lock -> lock();
			unit -> status = 1;
			//unit -> work_lock -> unlock();
		}
		if (unit)
		{
			done = false;
			execute_unit(unit);
			//unit -> work_lock -> lock();
			unit -> status = 2;
			//unit -> work_lock -> unlock();
		}
		thread_data -> vector_lock -> unlock();
	} while(!done);
	return NULL;
}

void do_work(std::vector<work_unit_t *> *work, std::mutex *vector_lock, size_t nthreads)
{
	pthread_t *threads = new pthread_t[nthreads];
	thread_data_t *thread_data = new thread_data_t[nthreads];
	for (unsigned int i = 0; i < nthreads; i++)
	{
		thread_data[i].work = work;
		thread_data[i].vector_lock = vector_lock;
		create_thread(&threads[i], NULL, work_thread, (void *) &thread_data[i]);
	}
	for (unsigned int i = 0; i < nthreads; i++)
	{
		pthread_join(threads[i], NULL);
	}
}

int main(int argc, char** argv)
{
	std::vector<work_unit_t*> work;
	std::mutex vector_lock;
	size_t buf_size = 1024;
	char* buf = new char[buf_size];
	unsigned int threads = 1;
	if (argc > 1)
	{
		strcpy(buf, argv[1]);
	}
	else if (getcwd(buf, buf_size) == NULL)
	{
		std::cerr << std::strerror(errno) << std::endl;
		return errno;
	}
	if (argc > 2)
	{
		threads = atoi(argv[2]);
	}
	DIR *dir = opendir(buf);
	make_work(&work, dir, buf);
	std::cout << "Work size: " << work.size() << std::endl;
	do_work(&work, &vector_lock, threads);
	delete[] buf;
	return 0;
}
