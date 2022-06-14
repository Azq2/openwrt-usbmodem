#include "UsbDiscover.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <optional>

#include <unistd.h>
#include <sys/file.h>
#include <sys/statvfs.h>

#include <Core/Uci.h>
#include <Core/Log.h>
#include <Core/Json.h>
#include <Core/Crc32.h>
#include <Core/Utils.h>

std::map<std::string, FILE *> UsbDiscover::m_locks = {};

std::string UsbDiscover::getLockPath(const std::string &dev) {
	auto real_path = getRealPath(dev);
	auto crc = crc32(0, reinterpret_cast<const uint8_t *>(real_path.c_str()), real_path.size());
	return strprintf("/var/run/usbmodem-%zu-%x-%s.lock", real_path.size(), crc, getFileBaseName(real_path).c_str());
}

bool UsbDiscover::tryLockDevice(const std::string &dev) {
	std::string path = getLockPath(dev);
	
	FILE *fp = fopen(path.c_str(), "w+");
	if (!fp)
		return false;
	
	if (flock(fileno(fp), LOCK_EX | LOCK_NB) != 0) {
		fclose(fp);
		return false;
	}
	
	m_locks[path] = fp;
	
	return true;
}

bool UsbDiscover::unlockDevice(const std::string &dev) {
	std::string path = getLockPath(dev);
	
	if (m_locks.find(path) != m_locks.end()) {
		flock(fileno(m_locks[path]), LOCK_UN);
		fclose(m_locks[path]);
		m_locks.erase(path);
	}
	
	return true;
}

bool UsbDiscover::isDeviceLocked(const std::string &dev) {
	std::string path = getLockPath(dev);
	
	FILE *fp = fopen(path.c_str(), "r");
	if (!fp)
		return false;
	
	if (flock(fileno(fp), LOCK_EX | LOCK_NB) != 0) {
		fclose(fp);
		return true;
	}
	
	flock(fileno(fp), LOCK_UN);
	fclose(fp);
	
	return false;
}

std::string UsbDiscover::mkUsbUrl(const DevUrl &url) {
	std::string url_str = strprintf("usb://%04x:%04x", url.vid, url.pid);
	if (url.params.size()) {
		std::vector<std::string> pairs;
		for (auto &it: url.params)
			pairs.push_back(urlencode(it.first) + "=" + urlencode(it.second));
		std::sort(pairs.begin(), pairs.end(), fileNameCmp);
		url_str += "/?" + strJoin("&", pairs);
	}
	return url_str;
}

std::pair<bool, UsbDiscover::DevUrl> UsbDiscover::parseUsbUrl(const std::string &url) {
	DevUrl result;
	
	if (!strStartsWith(url, "usb://")) {
		LOGE("Invalid usb url: %s (proto)\n", url.c_str());
		return {false, {}};
	}
	
	auto parts = strSplit("?", url.substr(strlen("usb://")));
	auto path_parts = strSplit("/", parts[0]);
	
	// vid:pid
	auto host_parts = strSplit(":", path_parts[0]);
	if (host_parts.size() != 2) {
		LOGE("Invalid usb url: %s (host)\n", url.c_str());
		return {false, {}};
	}
	
	result.vid = strToInt(host_parts[0], 16, 0);
	result.pid = strToInt(host_parts[1], 16, 0);
	
	// Custom params
	if (parts.size() > 1) {
		for (auto &pair: strSplit("&", parts[1])) {
			auto pair_parts = strSplit("=", pair);
			if (pair_parts.size() > 1) {
				result.params[urldecode(pair_parts[0])] = urldecode(pair_parts[1]);
			} else {
				result.params[urldecode(pair_parts[0])] = "";
			}
		}
	}
	
	return {true, result};
}

std::vector<std::string> UsbDiscover::getUsbDevicesByUrl(const DevUrl &url) {
	std::vector<std::string> devices;
	
	for (auto &path: readDir("/sys/bus/usb/devices")) {
		if (!isFile(path + "/idVendor") || !isFile(path + "/idProduct"))
			continue;
		
		std::string serial = trim(tryReadFile(path + "/serial"));
		uint16_t vid = strToInt(trim(tryReadFile(path + "/idVendor")), 16);
		uint16_t pid = strToInt(trim(tryReadFile(path + "/idProduct")), 16);
		
		if (vid != url.vid || pid != url.pid)
			continue;
		
		if (hasMapKey(url.params, "serial") && url.params.at("serial") != serial)
			continue;
		
		devices.push_back(path);
	}
	
	return devices;
}

std::pair<bool, UsbDiscover::Dev> UsbDiscover::findDevice(const std::string &url) {
	auto [valid, parsed_url] = parseUsbUrl(url);
	if (!valid)
		return {false, {}};
	
	for (auto &path: getUsbDevicesByUrl(parsed_url)) {
		bool used = false;
		auto [net_list, tty_list] = getUsbDevInterfaces(path);
		
		for (auto &tty: tty_list) {
			if (isDeviceLocked("/dev/" + tty.name)) {
				used = true;
				break;
			}
		}
		
		for (auto &net: net_list) {
			if (isDeviceLocked(net.name)) {
				used = true;
				break;
			}
		}
		
		if (!used)
			return {true, {path, net_list, tty_list}};
	}
	
	return {false, {}};
}

std::string UsbDiscover::getFromDevice(const Dev &dev, const std::string &path) {
	if (strStartsWith(path, "tty")) {
		int id = strToInt(path.substr(3), 10, -1);
		if (id < 0 || id >= dev.tty.size())
			return "";
		return "/dev/" + dev.tty[id].name;
	} else if (strStartsWith(path, "net")) {
		int id = strToInt(path.substr(3), 10, -1);
		if (id < 0 || id >= dev.net.size())
			return "";
		return dev.net[id].name;
	}
	return "";
}

UsbDiscover::ModemType UsbDiscover::getModemTypeFromString(const std::string &type) {
	for (int i = TYPE_UNKNOWN; i < TYPE__MAX; i++) {
		if (type == getEnumName(static_cast<ModemType>(i)))
			return static_cast<ModemType>(i);
	}
	return TYPE_UNKNOWN;
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

std::pair<std::vector<UsbDiscover::DevItem>, std::vector<UsbDiscover::DevItem>> UsbDiscover::getUsbDevInterfaces(const std::string &path) {
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
	
	auto *descr = findModemDescr(vid, pid);
	
	auto [net_list, tty_list] = getUsbDevInterfaces(path);
	
	if (!tty_list.size())
		return;
	
	bool found_modem = false;
	
	if (descr) {
		found_modem = true;
		if (descr->tty_control >= tty_list.size())
			found_modem = false;
		if (descr->tty_data >= tty_list.size())
			found_modem = false;
		if (descr->hasNetDev() && !net_list.size())
			found_modem = false;
	}
	
	DevUrl url = {};
	url.vid = vid;
	url.pid = pid;
	url.params["tty_count"] = std::to_string(tty_list.size());
	if (descr && descr->hasNetDev())
		url.params["net_count"] = std::to_string(net_list.size());
	url.params["serial"] = serial;
	url.params["name"] = descr ? descr->name : name;
	
	json usb_info = {
		{"vid", vid},
		{"pid", pid},
		{"serial", serial},
		{"name", descr ? descr->name : name},
		{"url", mkUsbUrl(url)},
		{"net", json::array()},
		{"tty", json::array()},
		{"plugged", true}
	};
	
	for (auto &d: net_list) {
		usb_info["net"].push_back({
			{"path", strprintf("net%d", d.id)},
			{"name", d.name},
			{"title", d.title},
		});
	}
	
	for (auto &d: tty_list) {
		usb_info["tty"].push_back({
			{"path", strprintf("tty%d", d.id)},
			{"name", d.name},
			{"title", d.title},
		});
	}
	
	if (found_modem) {
		usb_info["modem"] = {
			{"type", getEnumName(descr->type)},
			{"net_type", getEnumName(descr->net)},
			{"control", strprintf("tty%d", descr->tty_control)},
			{"data", strprintf("tty%d", descr->tty_data)}
		};
		
		if (descr->hasNetDev())
			usb_info["modem"]["net"] = "net0";
	}
	
	main_json["usb"].push_back(usb_info);
}

void UsbDiscover::discoverConfig(json &main_json) {
	auto sections = Uci::loadSections("network", "interface");
	for (auto &section: sections) {
		if (getMapValue(section.options, "proto", "") != "usbmodem")
			continue;
		
		auto device_url = getMapValue(section.options, "device", "");
		auto [valid, url] = parseUsbUrl(device_url);
		
		if (!valid)
			continue;
		
		std::string found;
		for (auto &usb: main_json["usb"]) {
			if (url.vid != usb["vid"] || url.pid != usb["pid"])
				continue;
			
			if (hasMapKey(url.params, "serial") && usb["serial"] != url.params["serial"])
				continue;
			
			found = usb["url"];
			break;
		}
		
		if (found.size()) {
			if (found != device_url)
				main_json["alias"][device_url] = found;
			continue;
		}
		
		auto *descr = findModemDescr(url.vid, url.pid);
		
		json usb_info = {
			{"vid", url.vid},
			{"pid", url.pid},
			{"serial", getMapValue(url.params, "serial", "")},
			{"name", getMapValue(url.params, "name", (descr ? descr->name : "Unknown Modem"))},
			{"url", device_url},
			{"net", json::array()},
			{"tty", json::array()},
			{"plugged", false}
		};
		
		int tty_count = strToInt(getMapValue(url.params, "tty_count", "0"), 10, 0);
		for (auto i = 0; i < tty_count; i++) {
			usb_info["tty"].push_back({
				{"path", strprintf("tty%d", i)},
				{"name", ""},
				{"title", ""},
			});
		}
		
		int net_count = strToInt(getMapValue(url.params, "net_count", "0"), 10, 0);
		for (auto i = 0; i < net_count; i++) {
			usb_info["net"].push_back({
				{"path", strprintf("net%d", i)},
				{"name", ""},
				{"title", ""},
			});
		}
		
		bool found_modem = false;
		
		if (descr) {
			found_modem = true;
			if (descr->tty_control >= tty_count)
				found_modem = false;
			if (descr->tty_data >= tty_count)
				found_modem = false;
			if (descr->hasNetDev() && !net_count)
				found_modem = false;
		}
		
		if (found_modem) {
			usb_info["modem"] = {
				{"type", getEnumName(descr->type)},
				{"net_type", getEnumName(descr->net)},
				{"control", strprintf("tty%d", descr->tty_control)},
				{"data", strprintf("tty%d", descr->tty_data)}
			};
			
			if (descr->hasNetDev())
				usb_info["modem"]["net"] = "net0";
		}
		
		main_json["usb"].push_back(usb_info);
	}
}

json UsbDiscover::discover() {
	json main_json = {
		{"usb", json::array()},
		{"tty", json::array()},
		{"alias", json::object()},
	};
	for (auto &dir: readDir("/sys/bus/usb/devices")) {
		if (isFile(dir + "/idVendor") && isFile(dir + "/idProduct"))
			discoverModem(main_json, dir);
	}
	
	discoverConfig(main_json);
	
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
	
	std::vector<std::string> net;
	for (auto &name: readDir("/sys/class/net")) {
		std::string devtype = "ethernet";
		std::string ifname = getFileBaseName(name);
		
		auto uevent = tryReadFile("/sys/class/net/" + ifname + "/uevent", "");
		size_t index = uevent.find("DEVTYPE=");
		if (index != std::string::npos) {
			index += strlen("DEVTYPE=");
			size_t eol_index = uevent.find("\n", index);
			if (eol_index != std::string::npos) {
				devtype = uevent.substr(index, eol_index - index);
			} else {
				devtype = uevent.substr(index);
			}
		}
		
		if (devtype == "ethernet" && ifname != "lo")
			net.push_back(ifname);
	}
	
	std::sort(net.begin(), net.end(), fileNameCmp);
	main_json["net"] = net;
	
	return main_json;
}

int UsbDiscover::run(const std::string &type, int argc, char *argv[]) {
	if (type == "discover") {
		json result = discover();
		
		int i = 0;
		for (auto &usb: result["usb"]) {
			printf("\n# %s\n", usb["name"].get<std::string>().c_str());
			
			if (hasMapKey(usb, "modem")) {
				if (i > 0) {
					printf("config interface 'mobile%d'\n", i);
				} else {
					printf("config interface 'mobile'\n");
				}
				
				printf("\toption proto 'usbmodem'\n");
				printf("\toption modem_type '%s'\n", usb["modem"]["type"].get<std::string>().c_str());
				printf("\toption device '%s'\n", usb["url"].get<std::string>().c_str());
				
				if (hasMapKey(usb["modem"], "control"))
					printf("\toption control_device '%s'\n", usb["modem"]["control"].get<std::string>().c_str());
				
				if (hasMapKey(usb["modem"], "data") && usb["modem"]["data"] != usb["modem"]["control"])
					printf("\toption ppp_device '%s'\n", usb["modem"]["data"].get<std::string>().c_str());
				
				if (hasMapKey(usb["modem"], "net"))
					printf("\toption net_device '%s'\n", usb["modem"]["net"].get<std::string>().c_str());
				
				if (usb["modem"]["type"] == "ppp")
					printf("\toption net_type '%s'\n", usb["modem"]["net_type"].get<std::string>().c_str());
				
				if (usb["modem"]["net_type"] == "cdma") {
					printf("\toption dialnumber '#777'\n");
				} else {
					if (usb["modem"]["type"] == "ppp")
						printf("\toption dialnumber '*99#'\n");
					printf("\toption apn 'internet'\n");
				}
				
				printf("#\toption username ''\n");
				printf("#\toption password ''\n");
			} else {
				printf("# Unknown modem! Please, manual configure other fields (modem_type, control_device, ppp_device and etc...).\n");
				
				if (i > 0) {
					printf("# config interface 'mobile%d'\n", i);
				} else {
					printf("# config interface 'mobile'\n");
				}
				
				printf("# \toption proto 'usbmodem'\n");
				printf("# \toption device '%s'\n", usb["url"].get<std::string>().c_str());
			}
			
			i++;
		}
	} else if (type == "discover-json") {
		printf("%s\n", discover().dump(2).c_str());
	}
	return 0;
}
