#include <iostream>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <termios.h>
#include <cstdarg>

termios* stdin_defaults = nullptr;
termios* stdout_defaults = nullptr;

inline void setting1()
{
	if (stdin_defaults == nullptr)
	{
		stdin_defaults = new termios;
	}
	::tcgetattr(0, stdin_defaults);
	struct termios settings = *stdin_defaults;
	settings.c_lflag &= (~ICANON);
	settings.c_lflag &= (~ECHO);
	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 1;
	::tcsetattr(0, TCSANOW, &settings);
}

inline void setting2()
{
	if (stdout_defaults == nullptr)
	{
		stdout_defaults = new termios;
	}
	::tcgetattr(1, stdout_defaults);
	struct termios settings = *stdout_defaults;
	settings.c_lflag &= (~ICANON);
	settings.c_lflag &= (~ECHO);
	settings.c_cc[VTIME] = 0;
	settings.c_cc[VMIN] = 1;
	::tcsetattr(1, TCSANOW, &settings);
}

inline void default_()
{
	if (stdin_defaults != nullptr)
	{
		::tcsetattr(0, TCSANOW, stdin_defaults);
		stdin_defaults = nullptr;
	}
	
	if (stdout_defaults != nullptr)
	{
		::tcsetattr(1, TCSANOW, stdout_defaults);
		stdout_defaults = nullptr;
	}
}

void error(const std::string& message)
{
	std::cerr << "\033[31m\033[1merror\033[0m : " << message << "\n";
	throw std::runtime_error(message);
}

void get_s_in_fmt(const std::string& str, const std::string& format, ...)
{
	va_list args;
	va_start(args, format);
	
	auto* result = new std::string;
	int pos_prefix_end = -1;
	for (int i = 0; i < format.size() && i < str.size(); ++i)
	{
		if (format[i] == '%' && format[i + 1] == 's')
		{
			pos_prefix_end = i;
			break;
		}
		else if (str[i] != format[i])
		{
			break;
		}
	}
	
	if (pos_prefix_end < 0)
	{
		error("prefix not found in \'str\'");
	}
	
	int j = pos_prefix_end;
	int i = pos_prefix_end + 2, pos_s = j;
	int delta = 0, delta2 = 0;
	for (; j < str.size() && i < format.size();)
	{
		bool is_eq = true;
		for (int k = i, l = j;; ++k, ++l)
		{
			if ((format.size() - k >= 2 && format[k] == '%' && format[k + 1] == 's') || k >= format.size())
			{
				delta2 += k - i;
				j = l;
				break;
			}
			else if (str[l] != format[k])
			{
				is_eq = false;
				break;
			}
		}
		if (is_eq)
		{
			*va_arg(args, std::string*) = str.substr(pos_s, delta);
			i += delta2 + 2;
			delta = 0;
			delta2 = 0;
			pos_s = j;
		}
		else
		{
			++delta;
			++j;
		}
	}
	if (format[i - 2] == '%' && format[i - 1] == 's')
	{
		*va_arg(args, std::string*) = str.substr(j);
	}
	
	va_end(args);
}

std::string& get_filename(const std::string& path)
{
	for (int i = path.size() - 1; i >= 0; --i)
	{
		if (path[i] == '/')
		{
			return *new std::string(path.substr(i + 1));
		}
	}
	return *new std::string;
}

bool ask_for_deletion(const char* what)
{
	std::cout << "\033[36m" << what << "\033[0m will be \033[31mdeleted\033[0m. Sure?(Y/n): ";
	char c = std::cin.get();
	if (c != '\n') std::cout << c << "\n";
	else std::cout << '\n';
	return c != 'n' || c != 'N';
}

void add_file_to_repo(const char* path)
{
	setting1();
	
	std::string& filename = get_filename(path);
	
	std::string package_name, epoch, version, release, package_arch;
	get_s_in_fmt(filename.c_str(), "%s-%s:%s-%s-%s.pkg.tar.zst", &package_name, &epoch, &version, &release, &package_arch);
	
	int start = 0;
	bool num = false;
	for (int i = 0; i < epoch.size(); ++i)
	{
		if (epoch[i] < '0' || epoch[i] > '9')
		{
			num = false;
			start = -1;
		}
		else
		{
			if (!num)
			{
				start = i;
				num = true;
			}
		}
	}
	
	if (start > 0)
	{
		package_name += '-';
		package_name += epoch.substr(0, start);
		package_name.pop_back();
		epoch = epoch.substr(start);
	}
	
	//if (ask_for_deletion((package_name + " files").c_str()))
	{
		system(("fish -c \"rm -rf *\'" + package_name + "\'*.pkg.tar.zst; git commit -a -m \'removed old " + package_name + " package\'\"").c_str());
	}
	
	
	int input, output;
	if ((input = open(path, O_RDONLY)) == -1)
	{
		std::cerr << "opening " << path << " as input failed : " << ::strerror(errno) << "\n";
		exit(-1);
	}
	
	struct stat st{ };
	if (::stat(filename.c_str(), &st) >= 0)
	{
		::remove(filename.c_str());
	}
	
	if ((output = creat(filename.c_str(), 0644)) == -1)
	{
		std::cerr << "opening " << filename << " as output failed : " << ::strerror(errno) << "\n";
		::close(input);
		exit(-1);
	}
	
	::fstat(input, &st);
	
	off_t offset = 0;
	
	if (::sendfile(output, input, &offset, st.st_size) == st.st_size)
	{
		std::cout << "copying file \033[32msuccessful\033[0m.\n";
	}
	else
	{
		std::cout << "copying file \033[31munsuccessful\033[0m.\n";
	}
	
	::close(input);
	::close(output);
	
	default_();
	
	system(("fish -c \"fish unconfigure.fish; repo-add messenger-repo.db.tar.gz \'" + filename
			+ "\'; fish configure.fish; git add *; git commit -a -m \'added " + filename + " package\'\"").c_str());
}

void include_file_to_repo(const char* path)
{
	system(("fish -c \"fish unconfigure.fish; repo-add messenger-repo.db.tar.gz \'" + std::string(path)
			+ "\'; fish configure.fish; git add *; git commit -a -m \'included " + std::string(path) + " package\'\"").c_str());
}

void delete_package_from_repo(const char* package_name)
{
	system(("fish -c \"git add *; fish unconfigure.fish; repo-remove messenger-repo.db.tar.gz \'"
			+ std::string(package_name) + "\'; fish configure.fish; rm -f \'" + std::string(package_name)
			+ "\'*.pkg.tar.zst; git commit -a -m \'removed " + package_name + " package\'\"").c_str());
}

int main(int argc, char** argv)
{
//	std::string& res = get_s_in_fmt("some text which must be splitted", "s%sme%swh%sch%ste");
//	std::cout << res;
	if (argc >= 3)
	{
		if (!::strcmp(argv[1], "add"))
		{
			for (int i = 2; i < argc; ++i)
			{
				add_file_to_repo(argv[i]);
			}
		}
		else if (!strcmp(argv[1], "del"))
		{
			for (int i = 2; i < argc; ++i)
			{
				delete_package_from_repo(argv[i]);
			}
		}
		else if (!strcmp(argv[1], "include"))
		{
			for (int i = 2; i < argc; ++i)
			{
				include_file_to_repo(argv[i]);
			}
		}
		system("git push");
		std::cout << "\n\033[32mgit \033[31mdiff\033[0m :\n";
		system("git diff");
	}
	else
	{
		std::cout << argv[0] << " add/del/include <package...>\n";
	}
}

