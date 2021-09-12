#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

#include "Json.h"
#include "Utils.h"
#include "UsbDiscover.h"

static json discoverModem(std::string path) {
	std::string name = trim(readFile(path + "/product"));
	std::string serial = trim(readFile(path + "/serial"));
	std::string vid = trim(readFile(path + "/idVendor"));
	std::string pid = trim(readFile(path + "/idProduct"));
	uint16_t bus_id = stoul(readFile(path + "/busnum"), 0, 16);
	uint16_t dev_id = stoul(readFile(path + "/devnum"), 0, 16);
	
	json modem = {
		{"name", name},
		{"vid", vid},
		{"pid", pid},
		{"bus", bus_id},
		{"dev", dev_id},
		{"serial", serial},
		{"tty", json::array()},
		{"net", json::array()},
		{"can_internet", false}
	};
	
	int tty_count = 0;
	int net_count = 0;
	
	for (auto &p: std::filesystem::directory_iterator(path)) {
		if (std::filesystem::exists(p.path().string() + "/tty")) {
			std::string dev_name;
			for (auto &tp: std::filesystem::directory_iterator(p.path().string() + "/tty")) {
				if (std::filesystem::exists(tp.path().string() + "/dev")) {
					dev_name =  tp.path().filename().c_str();
					break;
				}
			}
			
			if (!dev_name.size())
				continue;
			
			int iface = strToInt(readFile(p.path().string() + "/bInterfaceNumber"), 16);
			
			json tty = {
				{"device", dev_name},
				{"interface", iface},
				{"path", strprintf("usb://%s:%s/%d", vid.c_str(), pid.c_str(), iface)}
			};
			
			if (std::filesystem::exists(p.path().string() + "/interface"))
				tty["interface_name"] = trim(readFile(p.path().string() + "/interface"));
			
			modem["tty"].push_back(tty);
			tty_count++;
		}
		
		if (std::filesystem::exists(p.path().string() + "/net")) {
			std::string dev_name;
			for (auto &np: std::filesystem::directory_iterator(p.path().string() + "/net")) {
				if (std::filesystem::exists(np.path().string() + "/address")) {
					dev_name =  np.path().filename().c_str();
					break;
				}
			}
			
			int iface = strToInt(readFile(p.path().string() + "/bInterfaceNumber"), 16);
			
			json net = {
				{"device", dev_name},
				{"interface", iface},
				{"path", strprintf("usb://%s:%s/%d", vid.c_str(), pid.c_str(), iface)}
			};
			
			if (std::filesystem::exists(p.path().string() + "/interface"))
				net["interface_name"] = trim(readFile(p.path().string() + "/interface"));
			
			modem["net"].push_back(net);
			net_count++;
		}
	}
	
	if (!tty_count)
		return nullptr;
	
	if ((tty_count > 0 && net_count > 0) || tty_count > 1)
		modem["can_internet"] = true;
	
	return modem;
}

int checkDevice(int argc, char *argv[]) {
	if (argc == 3) {
		const char *device = argv[2];
		std::string tty_path = findTTY(device);
		printf("%d\n", tty_path.size() > 0 && std::filesystem::exists(tty_path) ? 1 : 0);
	} else {
		fprintf(stderr, "usage: %s check <device>\n", argv[0]);
	}
	return 0;
}

int findIfname(int argc, char *argv[]) {
	if (argc == 3) {
		const char *device = argv[2];
		std::string tty_path = findTTY(device);
		if (tty_path.size() > 0) {
			std::string net_device = findNetByTTY(tty_path);
			if (net_device.size() > 0)
				printf("%s\n", net_device.c_str());
		}
	} else {
		fprintf(stderr, "usage: %s iface <device>\n", argv[0]);
	}
	return 0;
}

int discoverUsbModems(int argc, char *argv[]) {
	json out = {
		{"modems", json::array()},
		{"tty", json::array()}
	};
	
	for (auto &p: std::filesystem::directory_iterator("/sys/bus/usb/devices")) {
		if (std::filesystem::exists(p.path().string() + "/idVendor") && std::filesystem::exists(p.path().string() + "/idProduct")) {
			json modem = discoverModem(p.path().string());
			if (modem != nullptr)
				out["modems"].push_back(modem);
		}
	}
	
	for (auto &p: std::filesystem::directory_iterator("/dev")) {
		if (p.path().filename().string().size() < 4)
			continue;
		
		if (strStartsWith(p.path().filename(), "tty"))
			out["tty"].push_back(p.path().string());
	}
	
	auto str = out.dump(1);
	printf("%s\n", str.c_str());
	
	return 0;
}
