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
		
		struct ModemTypeDescr {
			ModemType type;
			std::string name;
			std::vector<std::string> options;
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
	
	protected:
		static const std::vector<ModemDescr> m_modem_list;
		static const std::vector<ModemTypeDescr> m_types_descr;
	
	public:
		static int run(int argc, char *argv[]);
		static json discover();
		static json getModemTypes();
		
		static void discoverModem(json &main_json, const std::string &path);
		static const ModemDescr *findModemDescr(uint16_t vid, uint16_t pid);
		static std::pair<std::vector<DevItem>, std::vector<DevItem>> findDevices(const std::string &path);
		static const char *getEnumName(ModemType type);
		static const char *getEnumName(ModemNetworkType type);
};

int discoverUsbModems(int argc, char *argv[]);
int checkDevice(int argc, char *argv[]);
int findIfname(int argc, char *argv[]);
