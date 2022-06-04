#include "UsbDiscover.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <Core/Log.h>
#include <Core/Json.h>
#include <Core/Utils.h>

const std::vector<UsbDiscover::ModemTypeDescr> UsbDiscover::m_types_descr = {
	{
		UsbDiscover::TYPE_PPP,
		"PPP",
		{
			"modem_device",
			"ppp_device",
			"net_type",
			"pdp_type",
			"apn",
			"username",
			"password",
			"pin_code"
		}
	},
	{
		UsbDiscover::TYPE_ASR1802,
		"Marvell ASR1802",
		{
			"modem_device",
			"net_device",
			"pdp_type",
			"apn",
			"auth_type",
			"username",
			"password",
			"pin_code",
			"mep_code",
			"prefer_dhcp"
		}
	}
};

int UsbDiscover::run(int argc, char *argv[]) {
	if (!argc) {
		json result = discover();
		
		int i = 0;
		for (auto &modem: result["modems"]) {
			printf("\n# %s\n", modem["name"].get<std::string>().c_str());
			
			if (i > 0) {
				printf("config interface 'MODEM_%d'\n", i);
			} else {
				printf("config interface 'MODEM'\n");
			}
			
			printf("\toption proto 'usbmodem'\n");
			printf("\toption modem_type '%s'\n", modem["type"].get<std::string>().c_str());
			
			if (modem["type"] == "ppp")
				printf("\toption net_type '%s'\n", modem["net_type"].get<std::string>().c_str());
			
			printf("\toption device '%s'\n", modem["control"].get<std::string>().c_str());
			
			if (modem["data"] != modem["control"])
				printf("\toption ppp_device '%s'\n", modem["data"].get<std::string>().c_str());
			
			if (modem["net_type"] == "cdma") {
				printf("\toption dialnumber '#777'\n");
			} else {
				if (modem["type"] == "ppp")
					printf("\toption dialnumber '*99#'\n");
				printf("\toption apn 'internet'\n");
			}
			
			printf("#\toption username ''\n");
			printf("#\toption password ''\n");
		}
	} else if (!strcmp(argv[0], "types")) {
		printf("%s\n", getModemTypes().dump(2).c_str());
	} else if (!strcmp(argv[0], "modems")) {
		printf("%s\n", discover().dump(2).c_str());
	}
	return 0;
}

const char *UsbDiscover::getEnumName(ModemType type) {
	switch (type) {
		case TYPE_PPP:		return "ppp";
		case TYPE_ASR1802:	return "asr1802";
		case TYPE_MBIM:		return "mbim";
		case TYPE_QMI:		return "qmi";
		case TYPE_NCM:		return "ncm";
	}
	return "unknown";
}

const char *UsbDiscover::getEnumName(ModemNetworkType type) {
	switch (type) {
		case NET_GSM:	return "gsm";
		case NET_CDMA:	return "cdma";
	}
	return "unknown";
}

const UsbDiscover::ModemDescr *UsbDiscover::findModemDescr(uint16_t vid, uint16_t pid) {
	for (auto &descr: m_modem_list) {
		if (descr.vid == vid && descr.pid == pid)
			return &descr;
	}
	return nullptr;
}

json UsbDiscover::getModemTypes() {
	json result = json::array();
	for (auto &descr: m_types_descr) {
		result.push_back({
			{"name", descr.name},
			{"type", getEnumName(descr.type)},
			{"options", descr.options}
		});
	}
	return result;
}

json UsbDiscover::discover() {
	json main_json = {
		{"usb", json::array()},
		{"modems", json::array()},
		{"tty", json::array()},
	};
	for (auto &dir: readDir("/sys/bus/usb/devices")) {
		if (isFile(dir + "/idVendor") && isFile(dir + "/idProduct"))
			discoverModem(main_json, dir);
	}
	
	std::vector<std::string> tty;
	for (auto &name: readDir("/dev")) {
		auto base_name = getFileBaseName(name);
		if (strStartsWith(base_name, "tty") && base_name.size() > 3 && !isdigit(base_name[3])) {
			 tty.push_back(name);
		} else if (strStartsWith(base_name, "cdc-wdm")) {
			tty.push_back(name);
		}
	}
	
	std::sort(tty.begin(), tty.end(), fileNameCmp);
	main_json["tty"] = tty;
	
	return main_json;
}

std::pair<std::vector<UsbDiscover::DevItem>, std::vector<UsbDiscover::DevItem>> UsbDiscover::findDevices(const std::string &path) {
	std::vector<DevItem> net_list;
	std::vector<DevItem> tty_list;
	
	auto node_name = getFileBaseName(path);
	for (auto &ep_dir: readDir(path)) {
		if (!isDir(ep_dir))
			continue;
		
		if (!strStartsWith(getFileBaseName(ep_dir), node_name))
			continue;
		
		auto if_name = trim(tryReadFile(ep_dir + "/interface"));
		int if_num = strToInt(trim(tryReadFile(ep_dir + "/bInterfaceNumber")), 16);
		
		if (isDir(ep_dir + "/tty")) {
			for (auto &dir: readDir(ep_dir + "/tty")) {
				if (isFile(dir + "/dev")) {
					tty_list.push_back({0, if_num, getFileBaseName(dir), if_name});
					break;
				}
			}
		} else {
			for (auto &dir: readDir(ep_dir)) {
				if (isDir(dir + "/tty")) {
					tty_list.push_back({0, if_num, getFileBaseName(dir), if_name});
					break;
				}
			}
		}
		
		if (isDir(ep_dir + "/net")) {
			for (auto &dir: readDir(ep_dir + "/net"))
				net_list.push_back({0, if_num, getFileBaseName(dir), if_name});
		}
	}
	
	std::sort(net_list.begin(), net_list.end(), [](const auto &a, const auto &b) {
		return a.iface < b.iface;
	});
	
	std::sort(tty_list.begin(), tty_list.end(), [](const auto &a, const auto &b) {
		return a.iface < b.iface;
	});
	
	int net_index = 0;
	for (auto &dev: net_list)
		dev.id = net_index++;
	
	int tty_index = 0;
	for (auto &dev: tty_list)
		dev.id = tty_index++;
	
	return {net_list, tty_list};
}

void UsbDiscover::discoverModem(json &main_json, const std::string &path) {
	std::string name = trim(tryReadFile(path + "/product"));
	std::string serial = trim(tryReadFile(path + "/serial"));
	uint16_t vid = strToInt(trim(tryReadFile(path + "/idVendor")), 16);
	uint16_t pid = strToInt(trim(tryReadFile(path + "/idProduct")), 16);
	uint16_t bus = strToInt(trim(tryReadFile(path + "/busnum")), 10);
	uint16_t dev = strToInt(trim(tryReadFile(path + "/devnum")), 10);
	
	auto mk_dev_uri = [&](const DevItem &dev, const std::string &type) {
		return strprintf("usb://%04x:%04x/%s/%s%d", vid, pid, (serial.size() ? urlencode(serial).c_str() : "-"), type.c_str(), dev.id);
	};
	
	auto [net_list, tty_list] = findDevices(path);
	
	if (tty_list.size() > 0) {
		json usb_info = {
			{"vid", vid},
			{"pid", pid},
			{"bus", bus},
			{"dev", dev},
			{"serial", serial},
			{"name", name},
			{"net", json::array()},
			{"tty", json::array()},
		};
		
		for (auto &d: net_list) {
			usb_info["net"].push_back({
				{"url", mk_dev_uri(d, "net")},
				{"name", d.name},
				{"title", d.title},
			});
		}
		
		for (auto &d: tty_list) {
			usb_info["tty"].push_back({
				{"url", mk_dev_uri(d, "tty")},
				{"name", d.name},
				{"title", d.title},
			});
		}
		
		main_json["usb"].push_back(usb_info);
	}
	
	auto *descr = findModemDescr(vid, pid);
	if (descr) {
		bool valid = true;
		if (descr->tty_control >= tty_list.size())
			valid = false;
		if (descr->tty_data >= tty_list.size())
			valid = false;
		if (descr->hasNetDev() && !net_list.size())
			valid = false;
		
		if (valid) {
			json modem_descr = {
				{"vid", vid},
				{"pid", pid},
				{"bus", bus},
				{"dev", dev},
				{"serial", serial},
				{"name", descr->name},
				{"type", getEnumName(descr->type)},
				{"net_type", getEnumName(descr->net)},
				{"control", mk_dev_uri(tty_list[descr->tty_control], "tty")},
				{"data", mk_dev_uri(tty_list[descr->tty_data], "tty")}
			};
			
			if (descr->hasNetDev())
				modem_descr["net"] = mk_dev_uri(net_list[0], "net");
			
			main_json["modems"].push_back(modem_descr);
		}
	}
}

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
	/*
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
	*/
	auto str = UsbDiscover::discover().dump(2);
	printf("%s\n", str.c_str());
	
	return 0;
}
