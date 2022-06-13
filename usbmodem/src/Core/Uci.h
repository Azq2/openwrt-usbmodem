#pragma once

#include <string>
#include <map>
#include <vector>
#include <stdexcept>

class Uci {
	public:
		struct Section {
			std::string type;
			std::string name;
			std::map<std::string, std::string> options;
			std::map<std::string, std::vector<std::string>> lists;
		};
	
	public:
		static std::string getFirewallZone(const std::string &iface);
		static std::vector<Section> loadSections(const std::string &pkg_name, const std::string &type = "");
		static std::tuple<bool, Section> loadSectionByName(const std::string &pkg_name, const std::string &type, const std::string &name);
};
