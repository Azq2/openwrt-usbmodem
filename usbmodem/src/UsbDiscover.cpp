#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <json-c/json.h>

#include "Utils.h"
#include "UsbDiscover.h"

static void discoverModem(json_object *json_modems, std::string path) {
	std::string name = trim(readFile(path + "/product"));
	std::string serial = trim(readFile(path + "/serial"));
	std::string vid = trim(readFile(path + "/idVendor"));
	std::string pid = trim(readFile(path + "/idProduct"));
	uint16_t bus_id = stoul(readFile(path + "/busnum"), 0, 16);
	uint16_t dev_id = stoul(readFile(path + "/devnum"), 0, 16);
	
	json_object *modem = json_object_new_object();
	json_object *tty_list = json_object_new_array();
	json_object *net_list = json_object_new_array();
	
	if (!modem || !tty_list || !net_list)
		return;
	
	json_object_object_add(modem, "name", json_object_new_string(name.c_str()));
	json_object_object_add(modem, "vid", json_object_new_string(vid.c_str()));
	json_object_object_add(modem, "pid", json_object_new_string(pid.c_str()));
	json_object_object_add(modem, "bus", json_object_new_int(bus_id));
	json_object_object_add(modem, "dev", json_object_new_int(dev_id));
	json_object_object_add(modem, "serial", json_object_new_string(serial.c_str()));
	json_object_object_add(modem, "tty", tty_list);
	json_object_object_add(modem, "net", net_list);
	
	int tty_count = 0;
	int net_count = 0;
	
	char url[256];
	
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
			snprintf(url, sizeof(url), "usb://%s:%s/%d", vid.c_str(), pid.c_str(), iface);
			
			json_object *tty = json_object_new_object();
			if (!tty)
				return;
			
			if (std::filesystem::exists(p.path().string() + "/interface")) {
				std::string interface_name = trim(readFile(p.path().string() + "/interface"));
				json_object_object_add(tty, "interface_name", json_object_new_string(interface_name.c_str()));
			}
			
			json_object_object_add(tty, "device", json_object_new_string(dev_name.c_str()));
			json_object_object_add(tty, "interface", json_object_new_int(iface));
			json_object_object_add(tty, "path", json_object_new_string(url));
			json_object_array_add(tty_list, tty);
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
			snprintf(url, sizeof(url), "usb://%s:%s/%d", vid.c_str(), pid.c_str(), iface);
			
			json_object *net = json_object_new_object();
			if (!net)
				return;
			
			if (std::filesystem::exists(p.path().string() + "/interface")) {
				std::string interface_name = trim(readFile(p.path().string() + "/interface"));
				json_object_object_add(net, "interface_name", json_object_new_string(interface_name.c_str()));
			}
			
			json_object_object_add(net, "device", json_object_new_string(dev_name.c_str()));
			json_object_object_add(net, "interface", json_object_new_int(iface));
			json_object_object_add(net, "path", json_object_new_string(url));
			json_object_array_add(net_list, net);
			net_count++;
		}
	}
	
	if ((tty_count > 0 && net_count > 0) || tty_count > 1) {
		json_object_array_add(json_modems, modem);
	} else {
		json_object_put(modem);
	}
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
	json_object *json = json_object_new_object();
	json_object *modems = json_object_new_array();
	json_object *tty = json_object_new_array();
	
	if (!modems || !tty || !json)
		return -1;
	
	json_object_object_add(json, "modems", modems);
	json_object_object_add(json, "tty", tty);
	
	for (auto &p: std::filesystem::directory_iterator("/sys/bus/usb/devices")) {
		if (std::filesystem::exists(p.path().string() + "/idVendor") && std::filesystem::exists(p.path().string() + "/idProduct")) {
			discoverModem(modems, p.path().string());
		}
	}
	
	for (auto &p: std::filesystem::directory_iterator("/dev")) {
		if (p.path().filename().string().size() < 4)
			continue;
		
		if (strStartsWith(p.path().filename(), "tty"))
			json_object_array_add(tty, json_object_new_string(p.path().string().c_str()));
	}
	
	fprintf(stdout, "%s", json_object_to_json_string_ext(json, JSON_C_TO_STRING_PRETTY));
	json_object_put(json);
	return 0;
}
