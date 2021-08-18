#pragma once

#include <time.h>

#include <cmath>
#include <string>
#include <cerrno>
#include <cstdint>
#include <vector>

#define COUNT_OF(a) (sizeof((a)) / sizeof((a)[0]))

struct IpInfo {
	std::string ip;
	std::string mask;
	std::string gw;
	std::string dns1;
	std::string dns2;
};

int64_t getCurrentTimestamp();

inline int getNewTimeout(int64_t start, int timeout) {
	int64_t elapsed = (getCurrentTimestamp() - start);
	return elapsed > timeout ? 0 : timeout - elapsed;
}

inline int64_t timespecToMs(struct timespec *tm) {
	return ((int64_t) tm->tv_sec * 1000) + (int64_t) round((double) tm->tv_nsec / 1000000.0);
}

inline void msToTimespec(int64_t ms, struct timespec *tm) {
	int64_t seconds = ms / 1000;
	tm->tv_sec = seconds;
	tm->tv_nsec = (ms - (seconds * 1000)) * 1000000;
}

static inline bool strHasEol(const std::string &s) {
	return s.size() >= 2 && s[s.size() - 2] == '\r' && s[s.size() - 1] == '\n';
}

static inline bool strStartsWith(const std::string &a, const std::string &b) {
	return a.size() >= b.size() && memcmp(a.c_str(), b.c_str(), b.size()) == 0;
}

void setTimespecTimeout(struct timespec *tm, int timeout);

static inline int hex2byte(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return (c - 'A') + 10;
	if (c >= 'a' && c <= 'f')
		return (c - 'a') + 10;
	return -1;
}

std::string decodeUcs2(const std::string &data, bool be = false);
std::string decode7bit(const std::string &data);

int strToInt(const std::string &s, int base = 10, int default_value = 0);

std::string converOctalIpv6(const std::string &value);
int getIpType(const std::string &raw_ip, bool allow_dec_v6 = false);
bool normalizeIp(std::string *raw_ip, int require_ipv = 0, bool allow_dec_v6 = false);

int execFile(const std::string &path, std::vector<std::string> args, std::vector<std::string> envs);

std::string hex2bin(const std::string &hex);
std::string numberFormat(float num, int max_decimals = 0);
std::string numberFormat(double num, int max_decimals = 0);
std::string strJoin(const std::vector<std::string> &lines, const std::string &delim);
std::string strprintf(const char *format, ...);
std::string trim(std::string s);
std::string readFile(std::string path);
std::string findUsbIface(std::string dev_path, int iface);
std::string findUsbDevice(int vid, int pid);
std::string findUsbTTYName(const std::string &iface_path);
std::string findUsbNetName(const std::string &iface_path);
std::string findUsbTTY(int vid, int pid, int iface);
std::string findTTY(std::string url);
std::string findNetByTTY(std::string url);
std::string getDefaultNetmask(const std::string &ip);
