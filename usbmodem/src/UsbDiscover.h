#pragma once

#include <cstdint>
#include <stdexcept>

#include <Core/Json.h>

class UsbDiscover {
	public:
		enum ModemType: uint8_t {
			TYPE_UNKNOWN,
			TYPE_PPP,
			TYPE_ASR1802,
			TYPE_MBIM,
			TYPE_QMI,
			TYPE_NCM,
			TYPE__MAX
		};
		
		enum ModemNetworkType: uint8_t {
			NET_GSM,
			NET_CDMA
		};
		
		struct ModemDescr {
			uint16_t vid;
			uint16_t pid;
			std::string name;
			ModemType type;
			ModemNetworkType net;
			uint8_t tty_control;
			uint8_t tty_data;
			
			inline bool hasNetDev() const {
				return (type == TYPE_ASR1802 || type == TYPE_NCM);
			}
		};
		
		struct DevItem {
			int id;
			int iface;
			std::string name;
			std::string title;
		};
		
		struct DevUrl {
			uint16_t vid = 0;
			uint16_t pid = 0;
			std::map<std::string, std::string> params;
		};
		
		struct Dev {
			std::string path;
			std::vector<DevItem> net;
			std::vector<DevItem> tty;
		};
		
		static std::map<std::string, FILE *> m_locks;
	protected:
		static const std::vector<ModemDescr> m_modem_list;
	
	public:
		static int run(const std::string &type, int argc, char *argv[]);
		static json discover();
		static void discoverModem(json &main_json, const std::string &path);
		static void discoverConfig(json &main_json);
		
		/*
		 * Device URI
		 * */
		static std::string mkUsbUrl(const DevUrl &dev_url);
		static std::pair<bool, DevUrl> parseUsbUrl(const std::string &url);
		
		static std::pair<bool, Dev> findDevice(const std::string &url);
		static std::string getFromDevice(const Dev &dev, const std::string &path);
		
		/*
		 * Locks
		 * */
		static std::string getLockPath(const std::string &dev);
		static bool tryLockDevice(const std::string &dev);
		static bool isDeviceLocked(const std::string &dev);
		static bool unlockDevice(const std::string &dev);
		
		static std::vector<std::string> getUsbDevicesByUrl(const DevUrl &url);
		static std::pair<std::vector<DevItem>, std::vector<DevItem>> getUsbDevInterfaces(const std::string &path);
		
		static const ModemDescr *findModemDescr(uint16_t vid, uint16_t pid);
		
		/*
		 * Utils
		 * */
		static bool hasNetDev(ModemType type) {
			return (type == TYPE_NCM || type == TYPE_ASR1802);
		}
		
		static bool hasPppDev(ModemType type) {
			return (type == TYPE_PPP);
		}
		
		static bool hasControlDev(ModemType type) {
			return true;
		}
		
		static const char *getEnumName(ModemType type);
		static const char *getEnumName(ModemNetworkType type);
		static ModemType getModemTypeFromString(const std::string &type);
};
