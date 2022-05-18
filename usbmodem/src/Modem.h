#pragma once

#include <any>
#include <string>
#include <functional>

#include <Core/Log.h>
#include <Core/Events.h>

/*
 * Generic modem interface
 * */
class Modem {
	public:
		/*
		 * Network
		 * */
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
			NET_NOT_REGISTERED,
			NET_SEARCHING,
			NET_REGISTERED_HOME,
			NET_REGISTERED_ROAMING
		};
		
		struct NetworkSignal {
			float rssi_dbm;
			float bit_err_pct;
			float rscp_dbm;
			float ecio_db;
			float rsrq_db;
			float rsrp_dbm;
		};
		
		struct NetworkModeItem {
			int id;
			std::string name;
		};
		
		enum IfaceProto {
			IFACE_DHCP,
			IFACE_STATIC,
			IFACE_PPP
		};
		
		/*
		 * USSD
		 * */
		enum UssdCode: int {
			USSD_ERROR			= -1,
			USSD_OK				= 0,
			USSD_WAIT_REPLY		= 1,
			USSD_CANCELED		= 2
		};
		
		typedef std::function<void(UssdCode, const std::string &)> UssdCallback;
		
		/*
		 * SIM
		 * */
		enum SimState {
			SIM_NOT_INITIALIZED,
			SIM_NOT_SUPPORTED,
			SIM_READY,
			SIM_PIN1_LOCK,
			SIM_PIN2_LOCK,
			SIM_PUK1_LOCK,
			SIM_PUK2_LOCK,
			SIM_MEP_LOCK,
			SIM_OTHER_LOCK,
			SIM_WAIT_UNLOCK,
			SIM_ERROR
		};
		
		/*
		 * SMS
		 * */
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
		
		/*
		 * Operators
		 * */
		enum OperatorStatus {
			OPERATOR_STATUS_UNKNOWN		= 0,
			OPERATOR_STATUS_AVAILABLE	= 1,
			OPERATOR_STATUS_REGISTERED	= 2,
			OPERATOR_STATUS_FORBIDDEN	= 3,
		};
		
		enum OperatorRegStatus {
			OPERATOR_REG_NONE		= 0,
			OPERATOR_REG_AUTO		= 1,
			OPERATOR_REG_MANUAL		= 2,
		};
		
		struct Operator {
			NetworkTech tech;
			OperatorStatus status;
			OperatorRegStatus reg;
			std::string id;
			std::string name;
		};
		
		/*
		 * Info
		 * */
		struct IpInfo {
			std::string ip;
			std::string mask;
			std::string gw;
			std::string dns1;
			std::string dns2;
		};
		
		struct SimInfo {
			std::string number;
			std::string imsi;
			SimState state;
		};
		
		struct NetworkInfo {
			IpInfo ipv4;
			IpInfo ipv6;
			Operator oper;
			NetworkReg reg;
			NetworkTech tech;
			NetworkSignal signal;
		};
		
		struct ModemInfo {
			std::string imei;
			std::string vendor;
			std::string model;
			std::string version;
		};
		
		/*
		 * Events
		 * */
		
		// Event when connected to internetet
		struct EvDataConnected {
			// True, when connection changed without disconnection
			bool is_update;
			IpInfo ipv4;
			IpInfo ipv6;
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
		
		// Event when changed current operator
		struct EvOperatorChanged { };
		
		// Event when signal levels changed
		struct EvNetworkSignalChanged {
			NetworkSignal signal;
		};
		
		// Event when SIM status changed
		struct EvSimStateChaned {
			SimState state;
		};
		
		// Event when tty device is unrecoverable broken
		struct EvIoBroken { };
		
		// Event when connection timeout reached
		struct EvDataConnectTimeout { };
		
		Events m_ev;
		
		virtual bool setOption(const std::string &name, const std::any &value) = 0;
		
		/*
		 * Cache
		 * */
		struct CacheItem {
			std::any value;
			int version;
		};
		
		std::map<std::string, CacheItem> m_cache;
		
		std::any cached(const std::string &key, std::function<std::any()> callback, int version = 0);
		
		template <typename T>
		T cached(const std::string &key, std::function<std::any()> callback, int version = 0) {
			return std::any_cast<T>(cached(key, callback, version));
		}
	public:
		/*
		 * Main Operations
		 * */
		virtual bool open() = 0;
		virtual bool close() = 0;
		
		/*
		 * Info
		 * */
		virtual std::tuple<bool, ModemInfo> getModemInfo() = 0;
		virtual std::tuple<bool, SimInfo> getSimInfo() = 0;
		virtual std::tuple<bool, NetworkInfo> getNetworkInfo() = 0;
		
		/*
		 * Network
		 * */
		virtual std::vector<Operator> searchOperators() = 0;
		virtual bool setOperator(int mcc, int mnc, NetworkTech tech) = 0;
		
		virtual std::vector<NetworkModeItem> getNetworkModes() = 0;
		virtual bool setNetworkMode(int mode_id) = 0;
		
		virtual bool isRoamingEnabled() = 0;
		virtual bool setDataRoaming(bool enable) = 0;
		
		/*
		 * USSD
		 * */
		virtual bool sendUssd(const std::string &cmd, UssdCallback callback, int timeout) = 0;
		virtual bool cancelUssd() = 0;
		virtual bool isUssdBusy() = 0;
		virtual bool isUssdWaitReply() = 0;
		
		/*
		 * SMS
		 * */
		virtual std::tuple<bool, std::vector<Sms>> getSmsList(SmsDir from_dir) = 0;
		virtual bool deleteSms(int id) = 0;
		virtual SmsStorageCapacity getSmsCapacity() = 0;
		virtual SmsStorage getSmsStorage() = 0;
		
		/*
		 * Internals
		 * */
		virtual IfaceProto getIfaceProto() = 0;
		virtual int getDelayAfterDhcpRelease() = 0;
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout) = 0;
		
		/*
		 * Enums
		 * */
		static const char *getEnumName(NetworkTech tech, bool is_human_readable = false);
		static const char *getEnumName(NetworkReg reg, bool is_human_readable = false);
		static const char *getEnumName(SimState state, bool is_human_readable = false);
		
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
		 * Modem configuration
		 * */
		template <typename T>
		inline bool setOption(const std::string &name, const T &value) {
			auto any_value = std::make_any<T>(value);
			return setOption(name, any_value);
		}
};
