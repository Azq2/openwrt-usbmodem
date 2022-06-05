#pragma once

#include "BitEnums.h"

#include <time.h>

#include <cmath>
#include <string>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>

#define COUNT_OF(a) (sizeof((a)) / sizeof((a)[0]))
#define COUNT_OF_S(a) (static_cast<ssize_t>(sizeof((a)) / sizeof((a)[0])))

void setSignalHandler(int signal, const std::function<void(int)> &callback);

int64_t getCurrentTimestamp();

constexpr bool isLittleEndian() {
	return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
}

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

double rssiToPercent(double rssi, double min, double max);

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

int strToInt(const std::string &s, int base = 10, int default_value = 0);

std::string converOctalIpv6(const std::string &value);
int getIpType(const std::string &raw_ip, bool allow_dec_v6 = false);
bool normalizeIp(std::string *raw_ip, int require_ipv = 0, bool allow_dec_v6 = false);

std::pair<bool, std::string> tryHexToBin(const std::string &hex);

std::string bin2hex(const std::string &raw, bool uc = false);

std::string decodeBcd(const std::string &raw);

inline std::string hex2bin(const std::string &hex) {
	auto [success, decoded] = tryHexToBin(hex);
	return success ? decoded : hex;
}

std::string urlencode(const std::string &str);
std::string urldecode(const std::string &str);

std::string numberFormat(float num, int max_decimals = 0);
std::string numberFormat(double num, int max_decimals = 0);
std::string strJoin(const std::string &sep, const std::vector<std::string> &lines);
std::vector<std::string> strSplit(const std::string &sep, const std::string &str);
std::string strprintf(const char *format, ...)  __attribute__((format(printf, 1, 2)));
std::string trim(std::string s);
std::string getDefaultNetmask(const std::string &ip);

/*
 * File utils
 * */
std::string readFile(const std::string &path);
std::vector<std::string> readDir(const std::string &path);
bool isFileWriteable(const std::string &file);
bool isFileReadable(const std::string &file);
bool isFileExists(const std::string &file);
size_t getFileSize(const std::string &file);
bool isFileExists(const std::string &file);
int execFile(const std::string &path, std::vector<std::string> args, std::vector<std::string> envs);
bool isDir(const std::string &file);
bool isFile(const std::string &file);

inline std::string tryReadFile(const std::string &path, const std::string &def = "") {
	if (isFile(path) && isFileReadable(path))
		return readFile(path);
	return def;
}

inline std::string getFileBaseName(const std::string &path) {
	// Why argument non-const in basename()????
	return basename((char *) path.c_str());
}

bool fileNameCmp(const std::string &a, const std::string &b);
