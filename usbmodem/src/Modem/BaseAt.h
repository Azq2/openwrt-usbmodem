#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <map>

#include <Core/Serial.h>
#include <Core/AtChannel.h>
#include <Core/AtParser.h>
#include <Core/GsmUtils.h>

#include "../Modem.h"

/*
 * Base driver for any AT modem
 * */
class BaseAtModem: public Modem {
	protected:
		/*
		 * Current network registration status
		 * */
		enum CregStatus: int {
			CREG_NOT_REGISTERED					= 0,
			CREG_REGISTERED_HOME				= 1,
			CREG_SEARCHING						= 2,
			CREG_REGISTRATION_DENIED			= 3,
			CREG_UNKNOWN						= 4,
			CREG_REGISTERED_ROAMING				= 5,
			CREG_REGISTERED_HOME_SMS_ONLY		= 6,
			CREG_REGISTERED_ROAMING_SMS_ONLY	= 7,
			CREG_REGISTERED_EMERGENY_ONLY		= 8,
			CREG_REGISTERED_HOME_NO_CSFB		= 9,
			CREG_REGISTERED_ROAMING_NO_CSFB		= 10,
			CREG_REGISTERED_EMERGENY_ONLY2		= 11,
		};
		
		enum CregTech: int {
			CREG_TECH_GSM			= 0,
			CREG_TECH_GSM_COMPACT	= 1,
			CREG_TECH_UMTS			= 2,
			CREG_TECH_EDGE			= 3,
			CREG_TECH_HSDPA			= 4,
			CREG_TECH_HSUPA			= 5,
			CREG_TECH_HSPA			= 6,
			CREG_TECH_LTE			= 7,
			CREG_TECH_HSPAP			= 8,
			CREG_TECH_UNKNOWN		= 0xFF
		};
		
		struct Creg {
			CregStatus status = CREG_NOT_REGISTERED;
			CregTech tech = CREG_TECH_UNKNOWN;
			uint32_t loc_id = 0;
			uint32_t cell_id = 0;
			
			bool isRegistered() const;
			NetworkTech toNetworkTech() const;
			NetworkReg toNetworkReg() const;
		};
		
		enum SmsDir: uint8_t {
			SMS_DIR_UNREAD		= 0,
			SMS_DIR_READ		= 1,
			SMS_DIR_UNSENT		= 2,
			SMS_DIR_SENT		= 3,
			SMS_DIR_ALL			= 4,
		};
		
	protected:
		Serial m_serial;
		AtChannel m_at;
		
		bool m_self_test = false;
		
		int m_speed = 115200;
		std::string m_tty;
		
		std::string m_pdp_type;
		std::string m_pdp_apn;
		std::string m_pdp_auth_mode;
		std::string m_pdp_user;
		std::string m_pdp_password;
		
		std::string m_pincode;
		
		std::string m_modem_init = "";
	protected:
		BaseAtModem();
		
		/*
		 * Core Internals
		 * */
		virtual bool ping(int tries = 3);
		virtual bool handshake(int tries = 3);
		virtual int getCommandTimeout(const std::string &cmd);
		virtual bool customInit();
		
		bool execAtList(const char **commands, bool break_on_fail);
		
		/*
		 * SIM interbals
		 * */
		SimState m_sim_state = SIM_NOT_INITIALIZED;
		bool m_pincode_entered = false;
		
		void startSimPolling();
		void handleCpin(const std::string &event);
		void setSimState(SimState new_state);
		virtual bool handleSimLock(const std::string &code);
		
		/*
		 * Network internals
		 * */
		Creg m_creg = {};
		Creg m_cereg = {};
		Creg m_cgreg = {};
		
		IpInfo m_ipv4;
		IpInfo m_ipv6;
		
		NetworkSignal m_signal = {};
		NetworkTech m_tech = TECH_NO_SERVICE;
		NetworkReg m_net_reg = NET_NOT_REGISTERED;
		NetworkCell m_cell_info = {};
		
		std::string m_manual_creg_req;
		
		int m_net_cache_version = 0;
		bool m_allow_roaming = true;
		
		static NetworkTech cregToTech(CregTech creg_tech);
		static CregTech techToCreg(NetworkTech tech);
		
		virtual void handleCmt(const std::string &event);
		virtual void handleCsq(const std::string &event);
		virtual void handleCreg(const std::string &event);
		virtual void handleNetworkChange();
		
		NetworkTech getTechFromCops();
		
		/*
		 * USSD internals
		 * */
		uint32_t m_ussd_request_id = 0;
		int m_ussd_timeout = -1;
		bool m_ussd_session = false;
		UssdCallback m_ussd_callback;
		
		virtual void handleUssdResponse(int code, const std::string &data, int dcs);
		virtual void handleCusd(const std::string &event);
		
		/*
		 * SMS internals
		 * */
		bool m_sms_ready = false;
		SmsPreferredStorage m_sms_preferred_storage = SMS_PREFER_MODEM;
		bool m_storages_loaded = false;
		std::vector<SmsStorage> m_sms_all_storages[3];
		SmsStorage m_sms_mem[3] = {SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN};
		SmsStorageCapacity m_sms_capacity[3] = {};
		
		bool intiSms();
		bool syncSmsStorage();
		bool syncSmsCapacity();
		bool isSmsStorageSupported(int mem_id, SmsStorage check_storage);
		
		bool decodePduToSms(SmsDb::SmsType type, SmsDb::RawSms *sms, const std::string &hex, int index, bool is_unread);
		
		static std::string getSmsStorageName(SmsStorage storage);
		static SmsStorage getSmsStorageId(const std::string &name);
		
		bool discoverSmsStorages();
		bool findBestSmsStorage(bool prefer_sim);
		
		static inline float decodeSignal(int value, float from, float step = 1, int max = 99) {
			return -(value >= max ? NAN : from - (static_cast<float>(value) * step));
		}
		
		static inline float decodeRSSI(int rssi) {
			return -(rssi >= 99 ? NAN : 113 - rssi * 2);
		}
		
		static inline float decodeRSSI_V2(int rssi) {
			return -(rssi >= 99 ? NAN : 111 - rssi);
		}
		
		static inline float decodeRERR(int ber) {
			static const double bit_errors[] = {0.14, 0.28, 0.57, 1.13, 2.26, 4.53, 9.05, 18.10};
			return ber >= 0 && ber < COUNT_OF_S(bit_errors) ? bit_errors[ber] : NAN;
		}
		
		static inline float decodeRSCP(int rscp) {
			return -(rscp >= 255 ? NAN : 121 - rscp);
		}
		
		static inline float decodeECIO(int ecio) {
			return -(ecio >= 255 ? NAN : (49.0f - (float) ecio) / 2.0f);
		}
		
		static inline float decodeRSRQ(int rsrq) {
			return -(rsrq >= 255 ? NAN : (40.0f - (float) rsrq) / 2.0f);
		}
		
		static inline float decodeRSRP(int rsrp) {
			return -(rsrp >= 255 ? NAN : 141 - rsrp);
		}
		
		inline bool isPacketServiceReady() {
			return (m_net_reg == NET_REGISTERED_HOME || m_net_reg == NET_REGISTERED_ROAMING);
		}
		
		inline bool isRoamingNetwork() {
			return (m_net_reg == NET_REGISTERED_ROAMING);
		}
	public:
		virtual ~BaseAtModem() { }
		
		/*
		 * Main Operations
		 * */
		virtual bool init() = 0;
		virtual bool open() override;
		virtual bool close() override;
		
		/*
		 * Info
		 * */
		virtual std::tuple<bool, ModemInfo> getModemInfo() override;
		virtual std::tuple<bool, SimInfo> getSimInfo() override;
		virtual std::tuple<bool, NetworkInfo> getNetworkInfo() override;
		
		/*
		 * Network
		 * */
		virtual std::tuple<bool, std::vector<Operator>> searchOperators() override;
		virtual bool setOperator(OperatorRegMode mode, int mcc = -1, int mnc = -1, NetworkTech tech = TECH_UNKNOWN) override;
		
		virtual std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() override;
		virtual bool setNetworkMode(NetworkMode new_mode) override;
		virtual std::tuple<bool, NetworkMode> getCurrentNetworkMode() override;
		
		virtual std::tuple<bool, bool> isRoamingEnabled() override;
		virtual bool setDataRoaming(bool enable) override;
		
		virtual std::tuple<bool, Operator> getCurrentOperator() override;
		
		virtual std::tuple<bool, std::vector<NetworkNeighborCell>> getNeighboringCell() override;
		
		/*
		 * USSD
		 * */
		virtual bool sendUssd(const std::string &cmd, UssdCallback callback, int timeout) override;
		virtual bool cancelUssd() override;
		virtual bool isUssdBusy() override;
		virtual bool isUssdWaitReply() override;
		
		/*
		 * SMS
		 * */
		virtual bool deleteSms(int id) override;
		virtual bool deleteReadedSms() override;
		virtual SmsStorageCapacity getSmsCapacity() override;
		virtual SmsStorage getSmsStorage() override;
		virtual std::tuple<bool, std::vector<SmsDb::RawSms>> getSmsList(SmsListType list) override;
		
		/*
		 * Internals
		 * */
		virtual IfaceProto getIfaceProto() override;
		virtual int getDelayAfterDhcpRelease() override;
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout) override;
		virtual std::vector<Capability> getCapabilities() override;
		
		/*
		 * Modem configuration
		 * */
		virtual bool setOption(const std::string &name, const std::any &value) override;
};
