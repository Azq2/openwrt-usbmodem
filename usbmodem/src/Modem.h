#pragma once

#include <any>
#include <string>
#include <functional>

#include "Log.h"
#include "Events.h"

/*
 * Generic modem interface
 * */
class Modem {
	public:
		// Network technology
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
		
		// Network registration state
		enum NetworkReg {
			NET_NOT_REGISTERED		= 0,
			NET_SEARCHING			= 1,
			NET_REGISTERED_HOME		= 2,
			NET_REGISTERED_ROAMING	= 3,
		};
		
		// Network interface protocol
		enum IfaceProto {
			IFACE_DHCP		= 0,
			IFACE_STATIC	= 1,
			IFACE_PPP		= 2
		};
		
		// SIM PIN lock state
		enum PinState {
			PIN_UNKNOWN			= 0,
			PIN_REQUIRED		= 1,
			PIN_ERROR			= 2,
			PIN_READY			= 3,
			PIN_NOT_SUPPORTED	= 4
		};
		
		// Network signal levels
		struct SignalLevels {
			float rssi_dbm;
			float bit_err_pct;
			float rscp_dbm;
			float eclo_db;
			float rsrq_db;
			float rsrp_dbm;
		};
		
		// Interface IP info
		struct IpInfo {
			std::string ip;
			std::string mask;
			std::string gw;
			std::string dns1;
			std::string dns2;
		};
		
		// USSD response types
		enum UssdCode: int {
			USSD_ERROR			= -1,
			USSD_OK				= 0,
			USSD_WAIT_REPLY		= 1,
			USSD_CANCELED		= 2
		};
		
		typedef std::function<void(UssdCode, const std::string &)> UssdCallback;
		
		// SMS
		enum SmsStorage: uint8_t {
			SMS_STORAGE_MT			= 0,	// Sim + Phone
			SMS_STORAGE_ME			= 1,	// Phone
			SMS_STORAGE_SM			= 2,	// Sim
			SMS_STORAGE_UNKNOWN		= 0xFF
		};
		
		struct SmsStorageCapacity {
			int used;
			int total;
		};
		
		enum SmsDir: uint8_t {
			SMS_DIR_UNREAD		= 0,
			SMS_DIR_READ		= 1,
			SMS_DIR_UNSENT		= 2,
			SMS_DIR_SENT		= 3,
			SMS_DIR_ALL			= 4,
		};
		
		enum SmsType: uint8_t {
			SMS_INCOMING	= 0,
			SMS_OUTGOING	= 1
		};
		
		struct SmsPart {
			int id = -1;
			std::string text;
		};
		
		struct Sms {
			uint32_t hash = 0;
			SmsDir dir;
			SmsType type = SMS_INCOMING;
			bool invalid = false;
			bool unread = false;
			time_t time = 0;
			std::string addr;
			std::vector<SmsPart> parts;
		};
		
		typedef std::function<void(bool success, std::vector<Sms>)> SmsReadCallback;
		
		enum Features: uint32_t {
			FEATURE_USSD				= 1 << 0,
			FEATURE_SMS					= 1 << 1,
			FEATURE_PIN_STATUS			= 1 << 2,
			FEATURE_NETWORK_LEVELS		= 1 << 3,
		};
		
		/*
		 * Events
		 * */
		
		// Event when connected to internetet
		struct EvDataConnected {
			// True, when connection changed without disconnection
			bool is_update;
		};
		
		// Event when disconnected from internet
		struct EvDataDisconnected { };
		
		// Event when connecting to internet
		struct EvDataConnecting { };
		
		// Event when network technology changed
		struct EvTechChanged {
			const NetworkTech tech;
		};
		
		// Event when network registration state changed
		struct EvNetworkChanged {
			const NetworkReg status;
		};
		
		// Event when signal levels changed
		struct EvSignalLevels { };
		
		// Event when PIN status changed
		struct EvPinStateChaned {
			PinState state;
		};
		
		// Event when tty device is unrecoverable broken
		struct EvIoBroken { };
		
		// Event when connection timeout reached
		struct EvDataConnectTimeout { };
	protected:
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
		IpInfo m_ipv4 = {};
		IpInfo m_ipv6 = {};
		
		// Current signal info
		SignalLevels m_levels = {};
		
		// Current PIN state
		PinState m_pin_state = PIN_UNKNOWN;
		
		// Current network tech
		NetworkTech m_tech = TECH_NO_SERVICE;
		
		// Network registration status
		NetworkReg m_net_reg = NET_NOT_REGISTERED;
		
		// Modem identification
		std::string m_hw_vendor;
		std::string m_hw_model;
		std::string m_sw_ver;
		std::string m_imei;
		
		// Events interface
		Events m_ev;
	public:
		static const char *getTechName(NetworkTech tech);
		static const char *getNetRegStatusName(NetworkReg reg);
		
		Modem();
		virtual ~Modem();
		
		virtual bool open() = 0;
		virtual void close() = 0;
		virtual void finish() = 0;
		
		/*
		 * Current modem state
		 * */
		inline IpInfo getIpInfo(int ipv) const {
			return ipv == 6 ? m_ipv6 : m_ipv4;
		}
		
		inline SignalLevels getSignalLevels() const {
			return m_levels;
		}
		
		inline NetworkReg getNetRegStatus() const {
			return m_net_reg;
		}
		
		inline NetworkTech getTech() const {
			return m_tech;
		}
		
		inline PinState getPinState() const {
			return m_pin_state;
		}
		
		/*
		 * Modem configuration
		 * */
		virtual bool setCustomOption(const std::string &name, const std::any &value) = 0;
		
		template <typename T>
		inline bool setCustomOption(const std::string &name, const T &value) {
			auto any_value = std::make_any<T>(value);
			return setCustomOption(name, any_value);
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
		
		const inline std::string &getModel() const {
			return m_hw_model;
		}
		
		const inline std::string &getVendor() const {
			return m_hw_vendor;
		}
		
		const inline std::string &getSwVersion() const {
			return m_sw_ver;
		}
		
		const inline std::string &getImei() const {
			return m_imei;
		}
		
		/*
		 * Event interface
		 * */
		template <typename T>
		inline void on(const std::function<void(const T &)> &callback) {
			m_ev.on<T>(callback);
		}
		
		template <typename T>
		inline void emit(const T &value) {
			m_ev.emit(value);
		}
		
		/*
		 * Modem customizations
		 * */
		virtual IfaceProto getIfaceProto() = 0;
		virtual int getDelayAfterDhcpRelease();
		
		/*
		 * AT command API
		 * */
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout = 0) = 0;
		
		/*
		 * USSD API
		 * */
		virtual bool sendUssd(const std::string &cmd, UssdCallback callback, int timeout = 0) = 0;
		virtual bool cancelUssd() = 0;
		virtual bool isUssdBusy() = 0;
		virtual bool isUssdWaitReply() = 0;
		
		/*
		 * SMS API
		 * */
		virtual void getSmsList(SmsDir dir, SmsReadCallback callback) = 0;
		virtual bool deleteSms(int id) = 0;
		virtual SmsStorageCapacity getSmsCapacity() = 0;
		virtual SmsStorage getSmsStorage() = 0;
};
