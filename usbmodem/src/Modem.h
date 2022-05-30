#pragma once

#include <any>
#include <string>
#include <functional>
#include <cmath>

#include <Core/Log.h>
#include <Core/Events.h>
#include <Core/SmsDb.h>

/*
 * Generic modem interface
 * */
class Modem {
	public:
		/*
		 * Network
		 * */
		enum NetworkTech: int {
			TECH_UNKNOWN = -1,
			TECH_NO_SERVICE,
			TECH_GSM,
			TECH_GSM_COMPACT,
			TECH_GPRS,
			TECH_EDGE,
			TECH_UMTS,
			TECH_HSDPA,
			TECH_HSUPA,
			TECH_HSPA,
			TECH_HSPAP,
			TECH_LTE
		};
		
		enum NetworkReg {
			NET_NOT_REGISTERED,
			NET_SEARCHING,
			NET_REGISTERED_HOME,
			NET_REGISTERED_ROAMING
		};
		
		enum NetworkMode: int {
			NET_MODE_UNKNOWN = -1,
			
			NET_MODE_AUTO,
			
			NET_MODE_ONLY_2G,
			NET_MODE_ONLY_3G,
			NET_MODE_ONLY_4G,
			
			NET_MODE_PREFER_2G,
			NET_MODE_PREFER_3G,
			NET_MODE_PREFER_4G,
			
			NET_MODE_2G_3G_AUTO,
			NET_MODE_2G_3G_PREFER_2G,
			NET_MODE_2G_3G_PREFER_3G,
			
			NET_MODE_2G_4G_AUTO,
			NET_MODE_2G_4G_PREFER_2G,
			NET_MODE_2G_4G_PREFER_4G,
			
			NET_MODE_3G_4G_AUTO,
			NET_MODE_3G_4G_PREFER_3G,
			NET_MODE_3G_4G_PREFER_4G,
		};
		
		struct NetworkSignal {
			float rssi_dbm = NAN;
			float bit_err_pct = NAN;
			float rscp_dbm = NAN;
			float ecio_db = NAN;
			float rsrq_db = NAN;
			float rsrp_dbm = NAN;
			float div_rsrq_db = NAN;
			float div_rsrp_dbm = NAN;
			float main_rsrq_db = NAN;
			float main_rsrp_dbm = NAN;
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
			int used = 0;
			int total = 0;
		};
		
		enum SmsListType {
			SMS_LIST_ALL,
			SMS_LIST_UNREAD
		};
		
		/*
		 * Network operators
		 * */
		enum OperatorStatus {
			OPERATOR_STATUS_UNKNOWN		= 0,
			OPERATOR_STATUS_AVAILABLE	= 1,
			OPERATOR_STATUS_REGISTERED	= 2,
			OPERATOR_STATUS_FORBIDDEN	= 3,
		};
		
		struct NetworkCell {
			uint16_t loc_id = 0;
			uint16_t cell_id = 0;
		};
		
		enum OperatorRegMode {
			OPERATOR_REG_NONE		= 0,
			OPERATOR_REG_AUTO		= 1,
			OPERATOR_REG_MANUAL		= 2,
		};
		
		struct Operator {
			NetworkTech tech = TECH_NO_SERVICE;
			OperatorStatus status = OPERATOR_STATUS_UNKNOWN;
			OperatorRegMode reg = OPERATOR_REG_NONE;
			int mcc = 0;
			int mnc = 0;
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
			SimState state = SIM_NOT_INITIALIZED;
		};
		
		struct NetworkInfo {
			IpInfo ipv4;
			IpInfo ipv6;
			NetworkReg reg = NET_NOT_REGISTERED;
			NetworkTech tech = TECH_NO_SERVICE;
			NetworkSignal signal;
			Operator oper;
			NetworkCell cell;
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
		
		// Event when SIM status changed
		struct EvSimStateChaned {
			SimState state;
		};
		
		// Event when tty device is unrecoverable broken
		struct EvIoBroken { };
		
		// Event when connection timeout reached
		struct EvDataConnectTimeout { };
		
		// Event when SMS subsystem is ready
		struct EvSmsReady { };
		
		// Event when new SMS received
		struct EvNewDecodedSms {
			SmsDb::RawSms sms;
		};
		
		struct EvNewStoredSms {
			int index;
		};
		
		Events m_ev;
		
		virtual bool setOption(const std::string &name, const std::any &value) = 0;
		
		/*
		 * Custom Info
		 * */
		std::unordered_map<std::string, std::vector<std::pair<std::string, std::any>>> m_custom_info;
		
		/*
		 * Cache
		 * */
		struct CacheItem {
			std::any value;
			int version;
		};
		
		std::map<std::string, CacheItem> m_cache;
		
		std::tuple<bool, std::any> cached(const std::string &key, std::function<std::any()> callback, int version = 0);
		
		template <typename T>
		std::tuple<bool, T> cached(const std::string &key, std::function<std::any()> callback, int version = 0) {
			auto [status, value] = cached(key, callback, version);
			return {status, std::any_cast<T>(value)};
		}
	public:
		virtual ~Modem() { }
		
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
		inline std::unordered_map<std::string, std::vector<std::pair<std::string, std::any>>> getCustomInfo() {
			return m_custom_info;
		}
		
		/*
		 * Network
		 * */
		virtual std::tuple<bool, std::vector<Operator>> searchOperators() = 0;
		virtual bool setOperator(OperatorRegMode mode, int mcc = -1, int mnc = -1, NetworkTech tech = TECH_UNKNOWN) = 0;
		
		virtual std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() = 0;
		virtual std::tuple<bool, NetworkMode> getCurrentNetworkMode() = 0;
		virtual bool setNetworkMode(NetworkMode new_mode) = 0;
		
		virtual std::tuple<bool, bool> isRoamingEnabled() = 0;
		virtual bool setDataRoaming(bool enable) = 0;
		
		virtual std::tuple<bool, Operator> getCurrentOperator() = 0;
		
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
		virtual bool deleteSms(int id) = 0;
		virtual bool deleteReadedSms() = 0;
		virtual SmsStorageCapacity getSmsCapacity() = 0;
		virtual SmsStorage getSmsStorage() = 0;
		virtual std::tuple<bool, std::vector<SmsDb::RawSms>> getSmsList(SmsListType list) = 0;
		
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
		static const char *getEnumName(OperatorRegMode state, bool is_human_readable = false);
		static const char *getEnumName(OperatorStatus state, bool is_human_readable = false);
		static const char *getEnumName(NetworkMode state, bool is_human_readable = false);
		
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
