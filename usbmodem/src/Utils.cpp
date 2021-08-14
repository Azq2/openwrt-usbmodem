#include "Utils.h"

#include "Log.h"

#include <fstream>
#include <filesystem>
#include <algorithm> 
#include <cctype> 
#include <cmath> 
#include <arpa/inet.h>
#include <cstring>
#include <cstdarg>
#include <stdexcept>

using namespace std;

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
	
	return std::move(out);
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
	
	return std::move(out);
}

std::string converHexIpv6(const std::string &value) {
	return "";
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

std::string readFile(std::string path) {
	std::ifstream s(path);
	return std::string((std::istreambuf_iterator<char>(s)), std::istreambuf_iterator<char>());
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

std::string findUsbIface(std::string dev_path, int iface) {
	for (auto &p: std::filesystem::directory_iterator(dev_path)) {
		if (std::filesystem::exists(p.path().string() + "/bInterfaceNumber")) {
			int found_iface = strToInt(readFile(p.path().string() + "/bInterfaceNumber"), 16);
			if (found_iface == iface) {
				return p.path().string();
			}
		}
	}
	return "";
}

std::string findUsbDevice(int vid, int pid) {
	for (auto &p: std::filesystem::directory_iterator("/sys/bus/usb/devices")) {
		std::string path = p.path().string();
		if (std::filesystem::exists(path + "/idVendor") && std::filesystem::exists(path + "/idProduct")) {
			int found_vid = strToInt(readFile(path + "/idVendor"), 16);
			int found_pid = strToInt(readFile(path + "/idProduct"), 16);
			
			if (found_vid == vid && found_pid == pid)
				return path;
		}
	}
	return "";
}

std::string findUsbTTYName(const std::string &iface_path) {
	if (std::filesystem::exists(iface_path + "/tty")) {
		for (auto &p: std::filesystem::directory_iterator(iface_path + "/tty")) {
			if (std::filesystem::exists(p.path().string() + "/dev"))
				return p.path().filename();
		}
	}
	return "";
}

std::string findUsbNetName(const std::string &iface_path) {
	if (std::filesystem::exists(iface_path + "/net")) {
		for (auto &p: std::filesystem::directory_iterator(iface_path + "/net")) {
			if (std::filesystem::exists(p.path().string() + "/address"))
				return p.path().filename();
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

std::string findTTY(std::string url) {
	if (strStartsWith(url, "usb://")) {
		uint32_t vid, pid, iface;
		if (sscanf(url.c_str(), "usb://%x:%x/%u", &vid, &pid, &iface) == 3)
			return findUsbTTY(vid, pid, iface);
		return "";
	} else {
		return url;
	}
}

std::string findNetByTTY(std::string url) {
	if (!strStartsWith(url, "/dev/tty"))
		return "";
	
	std::string dev_name = std::filesystem::path(url).filename().string();
	std::string usb_dev_path = "/sys/class/tty/" + dev_name + "/device";
	
	for (auto &p: std::filesystem::directory_iterator(usb_dev_path + "/../")) {
		if (std::filesystem::exists(p.path().string() + "/bInterfaceNumber")) {
			std::string net_name = findUsbNetName(p.path().string());
			if (net_name.size() > 0)
				return net_name;
		}
	}
	
	return "";
}