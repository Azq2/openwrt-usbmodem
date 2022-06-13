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
	
	if (flock(fileno(fp), LOCK_NB) != 0) {
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
	
	if (flock(fileno(fp), LOCK_NB) != 0) {
		fclose(fp);
		return true;
	}
	
	flock(fileno(fp), LOCK_UN);
	fclose(fp);
	
	return false;
}

std::string UsbDiscover::mkUsbUrl(const UsbDevUrl &url) {
	std::string url_str = strprintf("usb://%04x:%04x", url.vid, url.pid);
	
	if (url.type == USB_DEV_URL_TTY) {
		url_str += "/tty" + std::to_string(url.id);
	} else if (url.type == USB_DEV_URL_NET) {
		url_str += "/net" + std::to_string(url.id);
	}
	
	if (url.params.size()) {
		std::vector<std::string> pairs;
		for (auto &it: url.params)
			pairs.push_back(urlencode(it.first) + "=" + urlencode(it.second));
		std::sort(pairs.begin(), pairs.end(), fileNameCmp);
		url_str += "?" + strJoin("&", pairs);
	}
	
	return url_str;
}

std::pair<bool, UsbDiscover::UsbDevUrl> UsbDiscover::parseUsbUrl(const std::string &url) {
	UsbDevUrl result;
	
	if (!strStartsWith(url, "usb://")) {
		LOGE("Invalid usb url: %s\n", url.c_str());
		return {false, {}};
	}
	
	auto parts = strSplit("?", url.substr(strlen("usb://")));
	auto path_parts = strSplit("/", parts[0]);
	
	// vid:pid
	auto host_parts = strSplit(":", path_parts[0]);
	if (host_parts.size() != 2) {
		LOGE("Invalid usb url: %s\n", url.c_str());
		return {false, {}};
	}
	
	result.vid = strToInt(host_parts[0], 16, 0);
	result.pid = strToInt(host_parts[1], 16, 0);
	
	// ttyN or netN
	if (path_parts.size() > 1) {
		if (strStartsWith(path_parts[1], "tty")) {
			result.type = USB_DEV_URL_TTY;
			result.id = strToInt(path_parts[1].substr(strlen("tty")));
		} else if (strStartsWith(path_parts[1], "net")) {
			result.type = USB_DEV_URL_NET;
			result.id = strToInt(path_parts[1].substr(strlen("net")));
		} else {
			LOGE("Invalid usb url: %s\n", url.c_str());
			return {false, {}};
		}
	}
	
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

bool UsbDiscover::isSameDevUrls(const std::vector<std::string> &urls) {
	std::optional<UsbDevUrl> last_url;
	for (auto &dev_url: urls) {
		if (!dev_url.size())
			continue;
		
		auto [valid, url] = parseUsbUrl(dev_url);
		if (last_url) {
			// Check USB vid:pid
			if (last_url->vid != url.vid || last_url->pid != url.pid)
				return false;
			
			// Check device S/N
			if (hasMapKey(last_url->params, "serial") != hasMapKey(url.params, "serial"))
				return false;
			if (getMapValue(last_url->params, "serial", "") != getMapValue(url.params, "serial", ""))
				return false;
		}
		last_url = url;
	}
	
	return last_url;
}

std::string UsbDiscover::getDevUniqId(const UsbDevUrl &url) {
	return strJoin("\0", {
		std::to_string(url.vid),
		std::to_string(url.pid),
		hasMapKey(url.params, "serial") ? "serial=" + url.params["serial"] : ""
	});
}

std::tuple<bool, std::vector<std::string>> UsbDiscover::resolveUrls(const std::vector<std::string> &urls) {
	std::map<std::string, std::string> usb_dev_cache;
	
	std::vector<std::string> result;
	for (auto i = 0; i < urls.size(); i++) {
		auto [valid, url] = parseUsbUrl(urls[i]);
		if (!valid) {
			result[i] = urls[i];
			continue;
		}
		
		
	}
	
	
	bool use_same = isSameDevUrls(urls);
	
	
	
	return {true, findUsbDevices(*last_url)};
}

std::vector<std::string> UsbDiscover::findUsbDevices(const UsbDevUrl &url) {
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

std::string UsbDiscover::findDevice(const std::string &url, UsbDevUrlType type) {
	if (strStartsWith(url, "usb://")) {
		auto [valid, parsed_url] = parseUsbUrl(url);
		if (!valid)
			return "";
		if (!valid || parsed_url.type != type)
			return "";
		
		auto devices = findUsbDevices(parsed_url);
		if (devices.size() > 0) {
			auto [net_list, tty_list] = findDevices(devices[0]);
			if (parsed_url.type == USB_DEV_URL_TTY) {
				if (parsed_url.id >= 0 && parsed_url.id < tty_list.size())
					return "/dev/" + tty_list[parsed_url.id].name;
			} else if (parsed_url.type == USB_DEV_URL_NET) {
				if (parsed_url.id >= 0 && parsed_url.id < net_list.size())
					return net_list[parsed_url.id].name;
			}
		}
		
		return "";
	}
	return url;
}

int UsbDiscover::run(const std::string &type, int argc, char *argv[]) {
	if (type == "discover") {
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
			
			i++;
		}
	} else if (type == "discover-json") {
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
	
	auto *descr = findModemDescr(vid, pid);
	
	auto mk_dev_uri = [&](const DevItem &dev, UsbDevUrlType type, int count) {
		UsbDevUrl url = {};
		url.vid = vid;
		url.pid = pid;
		url.type = type;
		url.id = dev.id;
		url.params["count"] = std::to_string(count);
		url.params["serial"] = serial;
		url.params["name"] = descr ? descr->name : name;
		return mkUsbUrl(url);
	};
	
	auto [net_list, tty_list] = findDevices(path);
	
	if (tty_list.size() > 0) {
		json usb_info = {
			{"vid", vid},
			{"pid", pid},
			{"bus", bus},
			{"dev", dev},
			{"serial", serial},
			{"name", descr ? descr->name : name},
			{"net", json::array()},
			{"tty", json::array()},
		};
		
		for (auto &d: net_list) {
			usb_info["net"].push_back({
				{"id", d.id},
				{"url", mk_dev_uri(d, USB_DEV_URL_NET, net_list.size())},
				{"name", d.name},
				{"title", d.title},
			});
		}
		
		for (auto &d: tty_list) {
			usb_info["tty"].push_back({
				{"id", d.id},
				{"url", mk_dev_uri(d, USB_DEV_URL_TTY, tty_list.size())},
				{"name", d.name},
				{"title", d.title},
			});
		}
		
		main_json["usb"].push_back(usb_info);
	}
	
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
				{"control", mk_dev_uri(tty_list[descr->tty_control], USB_DEV_URL_TTY, tty_list.size())},
				{"data", mk_dev_uri(tty_list[descr->tty_data], USB_DEV_URL_TTY, tty_list.size())}
			};
			
			if (descr->hasNetDev())
				modem_descr["net"] = mk_dev_uri(net_list[0], USB_DEV_URL_NET, net_list.size());
			
			main_json["modems"].push_back(modem_descr);
		}
	}
}
