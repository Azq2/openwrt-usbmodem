#include "Utils.h"

#include "Log.h"

#include <algorithm> 
#include <cctype> 
#include <cmath> 
#include <csignal>
#include <cstring>
#include <cstdarg>
#include <stdexcept>
#include <unordered_map>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>

using namespace std;

void setSignalHandler(int signal, const std::function<void(int)> &callback) {
	static std::unordered_map<int, std::function<void(int)>> signal_handlers;
	std::signal(signal, +[](int signal) {
		if (signal_handlers.find(signal) != signal_handlers.end())
			signal_handlers[signal](signal);
	});
	signal_handlers[signal] = callback;
}

int64_t getCurrentTimestamp() {
	struct timespec tm = {};
	
	int ret = clock_gettime(CLOCK_REALTIME, &tm);
	if (ret != 0)
		throw std::string("clock_gettime fatal error");
	
	return timespecToMs(&tm);
}

void setTimespecTimeout(struct timespec *tm, int timeout) {
	msToTimespec(getCurrentTimestamp() + timeout, tm);
}

int strToInt(const std::string &s, int base, int default_value) {
	try {
		return stoi(s, nullptr, base);
	} catch (std::invalid_argument &e) {
		return default_value;
	}
}

double rssiToPercent(double rssi, double min, double max) {
	double signal_quality = (rssi - min) / (max - min) * 100;
	return std::min(100.0, std::max(0.0, signal_quality));
}

std::string strJoin(const std::vector<std::string> &lines, const std::string &delim) {
	std::string out;
	size_t length = lines.size() > 1 ? delim.size() * (lines.size() - 1) : 0;
	for (auto &line: lines)
		length += line.size();
	
	out.reserve(length);
	
	bool first = true;
	for (auto &line: lines) {
		if (first) {
			first = false;
			out += line;
		} else {
			out += delim + line;
		}
	}
	
	return out;
}

std::string numberFormat(float num, int max_decimals) {
	float f1, f2;
	f2 = modff(num, &f1);
	
	long p1 = static_cast<long>(f1);
	long p2 = abs(static_cast<long>(f2 * (10 * max_decimals)));
	
	if (p2 != 0)
		return to_string(p1) + "." + to_string(p2);
	
	return to_string(p1);
}

std::string numberFormat(double num, int max_decimals) {
	double f1, f2;
	f2 = modf(num, &f1);
	
	long p1 = static_cast<long>(f1);
	long p2 = abs(static_cast<long>(f2 * (10 * max_decimals)));
	
	if (p2 != 0)
		return to_string(p1) + "." + to_string(p2);
	
	return to_string(p1);
}

std::string strprintf(const char *format, ...) {
	va_list v;
	
	std::string out;
	
	va_start(v, format);
	int n = vsnprintf(nullptr, 0, format, v);
	va_end(v);
	
	if (n <= 0)
		throw std::runtime_error("vsnprintf error...");
	
	out.resize(n);
	
	va_start(v, format);
	vsnprintf(&out[0], out.size() + 1, format, v);
	va_end(v);
	
	return out;
}

std::string bin2hex(const std::string &raw, bool uc) {
	static const char alpahbet_uc[] = "0123456789ABCDEF";
	static const char alpahbet_lc[] = "0123456789abcdef";
	
	std::string out;
	out.reserve(raw.size() * 2);
	
	for (auto c: raw) {
		uint8_t byte = static_cast<uint8_t>(c);
		if (uc) {
			out += alpahbet_uc[(byte >> 4) & 0xF];
			out += alpahbet_uc[(byte & 0xF)];
		} else {
			out += alpahbet_lc[(byte >> 4) & 0xF];
			out += alpahbet_lc[(byte & 0xF)];
		}
	}
	
	return out;
}

std::pair<bool, std::string> tryHexToBin(const std::string &hex) {
	if ((hex.size() % 2) != 0)
		return tryHexToBin("0" + hex.size());
	
	if (!hex.size())
		return std::make_pair(false, "");
	
	std::string out;
	out.reserve(hex.size() / 2);
	
	for (size_t i = 0; i < hex.size(); i += 2) {
		int upper = hex2byte(hex[i]);
		int lower = hex2byte(hex[i + 1]);
		
		if (upper < 0 || lower < 0)
			return std::make_pair(false, "");
		
		out += static_cast<char>((upper << 4) | lower);
	}
	
	return std::make_pair(true, out);
}

std::string decodeBcd(const std::string &raw) {
	static const char alphabet[] = "0123456789*#abc";
	
	std::string out;
	out.reserve(raw.size() * 2);
	
	for (auto c: raw) {
		uint8_t byte = static_cast<uint8_t>(c);
		
		if ((byte & 0xF) == 0xF)
			break;
		
		out += alphabet[byte & 0xF];
		
		if ((byte & 0xF0) == 0xF0)
			break;
		
		out += alphabet[(byte >> 4) & 0xF];
	}
	
	return out;
}

std::string converOctalIpv6(const std::string &value) {
	const char *cursor = value.c_str();
	std::string out;
	out.reserve(INET6_ADDRSTRLEN);
	
	char buf[4];
	
	for (int i = 0; i < 16; i++) {
		char *end = nullptr;
		long octet = strtol(cursor, &end, 10);
		
		// Not a number
		if (!end)
			return "";
		
		// Too long string
		if (i == 15 && *end)
			return "";
		
		// Valid delimiter not found
		if (i < 15 && *end != '.')
			return "";
		
		// Invalid cotets
		if (octet < 0 || octet > 255)
			return "";
		
		snprintf(buf, sizeof(buf), "%02X", (int) octet);
		
		if (((i % 2) == 0) && i > 0)
			out += ":";
		
		out += buf;
		
		cursor = end + 1;
	}
	
	LOGD("octal ipv6: %s -> %s\n", value.c_str(), out.c_str());
	
	return out;
}

std::string getDefaultNetmask(const std::string &ip) {
	in_addr addr;
	if (inet_aton(ip.c_str(), &addr) != 1)
		return "";
	
	uint8_t first_octet = (ntohl(addr.s_addr) >> 24) & 0xFF;
	
	// Cass A
	if (first_octet <= 127) {
		return "255.0.0.0";
	}
	// Class B
	else if (first_octet <= 191) {
		return "255.255.0.0";
	}
	// Class C
	else if (first_octet <= 223) {
		return "255.255.255.0";
	}
	
	return "";
}

int getIpType(const std::string &raw_ip, bool allow_dec_v6) {
	uint8_t buf[sizeof(struct in6_addr)];
	
	if (inet_pton(AF_INET, raw_ip.c_str(), buf) == 1) {
		return 4;
	} else if (inet_pton(AF_INET6, raw_ip.c_str(), buf) == 1) {
		return 6;
	} else if (allow_dec_v6) {
		return getIpType(converOctalIpv6(raw_ip), false);
	}
	
	return 0;
}

bool normalizeIp(std::string *raw_ip, int require_ipv, bool allow_dec_v6) {
	uint8_t buf[sizeof(struct in6_addr)];
	char str[INET6_ADDRSTRLEN];
	
	// IPv4
	if (inet_pton(AF_INET, raw_ip->c_str(), buf) == 1) {
		if (require_ipv && require_ipv != 4)
			return false;
		
		if (!inet_ntop(AF_INET, buf, str, INET6_ADDRSTRLEN))
			return false;
		raw_ip->assign(str, strlen(str));
		return true;
	}
	// IPv6
	else if (inet_pton(AF_INET6, raw_ip->c_str(), buf) == 1) {
		if (require_ipv && require_ipv != 6)
			return false;
		
		if (!inet_ntop(AF_INET6, buf, str, INET6_ADDRSTRLEN))
			return false;
		raw_ip->assign(str, strlen(str));
		return true;
	}
	// Decimal IPv6
	else if (allow_dec_v6 && (!require_ipv || require_ipv == 6)) {
		std::string normal_ip = converOctalIpv6(*raw_ip);
		if (normal_ip.size() > 0) {
			raw_ip->assign(normal_ip);
			return normalizeIp(raw_ip, require_ipv, false);
		}
	}
	
	return false;
}

std::string trim(std::string s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](uint8_t c) {
		return !isspace(c);
	}));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](uint8_t c) {
		return !isspace(c);
	}).base(), s.end());
	return s;
}

std::string urlencode(const std::string &str) {
	std::string result;
	for (auto c: str) {
		if (!isalpha(c) && !isdigit(c)) {
			result += strprintf("%%%02X", c);
		} else {
			result += c;
		}
	}
	return result;
}

std::string findUsbIface(const std::string &dev_path, int iface) {
	for (auto &dir: readDir(dev_path)) {
		if (isFile(dir + "/bInterfaceNumber")) {
			int found_iface = strToInt(readFile(dir + "/bInterfaceNumber"), 16);
			if (found_iface == iface)
				return dir;
		}
	}
	return "";
}

std::string findUsbDevice(int vid, int pid) {
	for (auto &dir: readDir("/sys/bus/usb/devices")) {
		if (isFile(dir + "/idVendor") && isFile(dir + "/idProduct")) {
			int found_vid = strToInt(readFile(dir + "/idVendor"), 16);
			int found_pid = strToInt(readFile(dir + "/idProduct"), 16);
			
			if (found_vid == vid && found_pid == pid)
				return dir;
		}
	}
	return "";
}

std::string findUsbTTYName(const std::string &iface_path) {
	if (isDir(iface_path + "/tty")) {
		for (auto &dir: readDir(iface_path + "/tty")) {
			if (isDir(dir + "/dev"))
				return getFileBaseName(dir);
		}
	}
	return "";
}

std::string findUsbNetName(const std::string &iface_path) {
	if (isDir(iface_path + "/net")) {
		for (auto &dir: readDir(iface_path + "/net")) {
			if (isDir(dir + "/address"))
				return getFileBaseName(dir);
		}
	}
	return "";
}

std::string findUsbTTY(int vid, int pid, int iface) {
	std::string dev_path = findUsbDevice(vid, pid);
	if (dev_path.size() > 0) {
		std::string iface_path = findUsbIface(dev_path, iface);
		if (iface_path.size() > 0) {
			std::string tty_name = findUsbTTYName(iface_path);
			if (tty_name.size() > 0)
				return "/dev/" + tty_name;
		}
	}
	return "";
}

std::string findTTY(const std::string &url) {
	if (strStartsWith(url, "usb://")) {
		uint32_t vid, pid, iface;
		if (sscanf(url.c_str(), "usb://%x:%x/%u", &vid, &pid, &iface) == 3)
			return findUsbTTY(vid, pid, iface);
		return "";
	} else {
		return url;
	}
}

std::string findNetByTTY(const std::string &url) {
	if (!strStartsWith(url, "/dev/tty"))
		return "";
	
	std::string dev_name = getFileBaseName(url);
	std::string usb_dev_path = "/sys/class/tty/" + dev_name + "/device";
	
	for (auto &dir: readDir(usb_dev_path + "/../")) {
		if (isFile(dir + "/bInterfaceNumber")) {
			std::string net_name = findUsbNetName(dir);
			if (net_name.size() > 0)
				return net_name;
		}
	}
	
	return "";
}

/*
 * File utils
 * */
std::string readFile(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");
	if (!fp)
		throw std::runtime_error(strprintf("fopen(%s) error: %s", path.c_str(), strerror(errno)));
	
	char buff[4096];
	std::string result;
	while (!feof(fp)) {
		int readed = fread(buff, 1, sizeof(buff), fp);
		if (readed > 0)
			result.append(buff, readed);
	}
	fclose(fp);
	
	return result;
}

std::vector<std::string> readDir(const std::string &path) {
	DIR *dir_p = opendir(path.c_str());
	if (!dir_p)
		throw std::runtime_error(strprintf("opendir(%s) error: %s", path.c_str(), strerror(errno)));
	
	std::vector<std::string> result;
	while (auto *ent = readdir(dir_p)) {
		if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
			continue;
		result.push_back(path + "/" + ent->d_name);
	}
	closedir(dir_p);
	
	return result;
}

size_t getFileSize(const std::string &file) {
	struct stat st;
	if (lstat(file.c_str(), &st) != 0)
		return 0;
	return st.st_size;
}

bool isFileExists(const std::string &file) {
	return access(file.c_str(), F_OK) == 0;
}

bool isFileReadable(const std::string &file) {
	return access(file.c_str(), F_OK | R_OK) == 0;
}

bool isFileWriteable(const std::string &file) {
	return access(file.c_str(), F_OK | W_OK) == 0;
}

bool isFile(const std::string &file) {
	struct stat st;
	if (lstat(file.c_str(), &st) != 0)
		return false;
	return (st.st_mode & S_IFMT) != S_IFDIR;
}

bool isDir(const std::string &file) {
	struct stat st;
	if (lstat(file.c_str(), &st) != 0)
		return false;
	return (st.st_mode & S_IFMT) == S_IFDIR;
}

int execFile(const std::string &path, std::vector<std::string> args, std::vector<std::string> envs) {
	// Arguments
	std::vector<char *> args_c_array;
	args_c_array.resize(args.size() + 1);
	for (auto &value: args)
		args_c_array.push_back(value.data());
	args_c_array.push_back(nullptr);
	
	// Env variables
	std::vector<char *> envs_c_array;
	envs_c_array.resize(envs.size() + 1);
	for (auto &value: envs)
		envs_c_array.push_back(value.data());
	envs_c_array.push_back(nullptr);
	
	return execvpe(path.c_str(), args_c_array.data(), envs_c_array.data());
}

bool fileNameCmp(const std::string &a, const std::string &b) {
	size_t len = std::max(a.size(), b.size());
	for (auto i = 0; i < len; i++) {
		auto a_ch = i < a.size() ? a[i] : 0;
		auto b_ch = i < b.size() ? b[i] : 0;
		
		if (a_ch > b_ch)
			return false;
	}
	return true;
}
