#pragma once

#include <vector>
#include <string>
#include <thread>

#include "Log.h"
#include "Loop.h"
#include "Serial.h"
#include "AtChannel.h"
#include "AtParser.h"

#include <arpa/inet.h>

class Modem {
	public:
		enum NetworkTech: int {
			TECH_UNKNOWN	= -1,
			TECH_NO_SERVICE	= 0,
			TECH_GSM		= 1,
			TECH_GPRS		= 2,
			TECH_EDGE		= 3,
			TECH_UMTS		= 4,
			TECH_HSDPA		= 5,
			TECH_HSUPA		= 6,
			TECH_HSPA		= 7,
			TECH_HSPAP		= 8,
			TECH_LTE		= 9
		};
		
		enum NetworkReg {
			NET_NOT_REGISTERED		= 0,
			NET_SEARCHING			= 1,
			NET_REGISTERED_HOME		= 2,
			NET_REGISTERED_ROAMING	= 3,
		};
		
		enum IfaceProto {
			IFACE_DHCP		= 0,
			IFACE_STATIC	= 1,
			IFACE_PPP		= 2
		};
		
		struct EvDataConnected {
			bool is_update;
		};
		
		struct EvDataDisconnected { };
		
		struct EvDataConnecting { };
		
		struct EvTechChanged {
			const NetworkTech tech;
		};
		
		struct EvNetworkChanged {
			const NetworkReg status;
		};
		
	protected:
		Serial m_serial;
		AtChannel m_at;
		std::thread *m_at_thread = nullptr;
		
		// Serial config
		int m_speed = 115200;
		std::string m_tty;
		std::string m_iface;
		
		// PDP config
		std::string m_pdp_type;
		std::string m_pdp_apn;
		std::string m_pdp_auth_mode;
		std::string m_pdp_user;
		std::string m_pdp_password;
		
		// Pin
		std::string m_pincode;
		
		// Current connection
		std::string m_ipv4_gw;
		std::string m_ipv4_ip;
		std::string m_ipv4_mask;
		std::string m_ipv4_dns1;
		std::string m_ipv4_dns2;
		
		std::string m_ipv6_gw;
		std::string m_ipv6_ip;
		std::string m_ipv6_mask;
		std::string m_ipv6_dns1;
		std::string m_ipv6_dns2;
		
		bool m_prefer_dhcp = false;
		bool m_force_restart_network = false;
		
		virtual bool init() = 0;
		virtual bool handshake();
		virtual int getDefaultAtTimeout();
		
		// Current network tech
		NetworkTech m_tech = TECH_NO_SERVICE;
		
		// Network reg status
		NetworkReg m_net_reg = NET_NOT_REGISTERED;
	public:
		Modem();
		virtual ~Modem();
		
		virtual IfaceProto getIfaceProto();
		virtual int getDelayAfterDhcpRelease();
		
		inline void setPreferDhcp(bool enable) {
			m_prefer_dhcp = enable;
		}
		
		inline void setForceNetworkRestart(bool enable) {
			m_force_restart_network = enable;
		}
		
		static const char *getTechName(NetworkTech tech);
		static const char *getNetRegStatusName(NetworkReg reg);
		
		bool getIpInfo(int ipv, IpInfo *ip_info) const;
		
		inline void setNetIface(const std::string &iface) {
			m_iface = iface;
		}
		
		inline void setPdpConfig(const std::string &pdp_type, const std::string &apn, const std::string &auth_mode, const std::string &user, const std::string &password) {
			m_pdp_type = pdp_type;
			m_pdp_apn = apn;
			m_pdp_auth_mode = auth_mode;
			m_pdp_user = user;
			m_pdp_password = password;
		}
		
		inline void setPinCode(const std::string &pincode) {
			m_pincode = pincode;
		}
		
		inline void setSerial(const std::string &tty, int speed) {
			m_tty = tty;
			m_speed = speed;
		}
		
		bool open();
		void close();
};
