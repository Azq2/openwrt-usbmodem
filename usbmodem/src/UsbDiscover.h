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
			TYPE_NCM
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
		
		enum UsbDevUrlType {
			USB_DEV_URL_TTY,
			USB_DEV_URL_NET
		};
		
		struct UsbDevUrl {
			UsbDevUrlType type = USB_DEV_URL_TTY;
			int id = 0;
			uint16_t vid = 0;
			uint16_t pid = 0;
			std::map<std::string, std::string> params;
		};
	protected:
		static const std::vector<ModemDescr> m_modem_list;
	
	public:
		static int run(int argc, char *argv[]);
		static json discover();
		
		static std::string mkUsbUrl(const UsbDevUrl &dev_url);
		static std::pair<bool, UsbDevUrl> parseUsbUrl(const std::string &url);
		
		static std::string findDevice(const std::string &url, UsbDevUrlType type);
		static inline std::string findTTY(const std::string &url) {
			return findDevice(url, USB_DEV_URL_TTY);
		}
		static inline std::string findNet(const std::string &url) {
			return findDevice(url, USB_DEV_URL_NET);
		}
		
		static void discoverModem(json &main_json, const std::string &path);
		static const ModemDescr *findModemDescr(uint16_t vid, uint16_t pid);
		static std::pair<std::vector<DevItem>, std::vector<DevItem>> findDevices(const std::string &path);
		static const char *getEnumName(ModemType type);
		static const char *getEnumName(ModemNetworkType type);
};
