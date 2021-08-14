#pragma once

#include <string>
#include <map>

class Uci {
	public:
		static bool loadIfaceConfig(const std::string &iface, std::map<std::string, std::string> *options);
		static bool loadIfaceFwZone(const std::string &iface, std::string *zone);
};
