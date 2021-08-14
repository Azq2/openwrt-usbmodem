#pragma once

#include <vector>
#include <string>
#include <pthread.h>

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
		
		enum PinState {
			PIN_UNKNOWN		= 0,
			PIN_REQUIRED	= 1,
			PIN_ERROR		= 2,
			PIN_READY		= 3
		};
		
		struct SignalLevels {
			float rssi_dbm;
			float bit_err_pct;
			float rscp_dbm;
			float eclo_db;
			float rsrq_db;
			float rsrp_dbm;
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
		
		struct EvSignalLevels { };
		
		struct EvPinStateChaned {
			PinState state;
		};
		
		struct EvIoBroken { };
	protected:
		Serial m_serial;
		AtChannel m_at;
		pthread_t m_at_thread = 0;
		bool m_at_thread_created = false;
		
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
		bool m_pincode_entered = false;
		
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
		
		float m_rssi_dbm = NAN;
		float m_bit_err_pct = NAN;
		float m_rscp_dbm = NAN;
		float m_eclo_db = NAN;
		float m_rsrq_db = NAN;
		float m_rsrp_dbm = NAN;
		
		bool m_prefer_dhcp = false;
		bool m_force_restart_network = false;
		
		virtual bool init() = 0;
		virtual bool handshake();
		virtual int getDefaultAtTimeout();
		
		static void *readerThread(void *arg);
		
		PinState m_pin_state = PIN_UNKNOWN;
		
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
		void getSignalLevels(SignalLevels *levels) const;
		
		inline NetworkReg getNetRegStatus() const {
			return m_net_reg;
		}
		
		inline NetworkTech getTech() const {
			return m_tech;
		}
		
		inline PinState getPinState() const {
			return m_pin_state;
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
		virtual void finish();
};
