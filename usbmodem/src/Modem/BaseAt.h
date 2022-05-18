#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <map>
#include "zlib.h"

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
			uint16_t loc_id = 0;
			uint16_t cell_id = 0;
			
			bool isRegistered() const;
			NetworkTech toNetworkTech() const;
			NetworkReg toNetworkReg() const;
		};
	
	protected:
		Serial m_serial;
		AtChannel m_at;
		
		bool m_self_test = false;
		
		int m_speed = 115200;
		std::string m_tty;
		std::string m_iface;
		
		std::string m_pdp_type;
		std::string m_pdp_apn;
		std::string m_pdp_auth_mode;
		std::string m_pdp_user;
		std::string m_pdp_password;
		
		std::string m_pincode;
	protected:
		BaseAtModem();
		
		/*
		 * Core Internals
		 * */
		virtual bool ping(int tries = 3);
		virtual bool handshake(int tries = 3);
		virtual int getCommandTimeout(const std::string &cmd);
		
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
		
		int m_connect_timeout = 0;
		int m_connect_timeout_id = -1;
		int m_net_cache_version = 0;
		
		void stopNetWatchdog();
		void startNetWatchdog();
		
		static NetworkTech cregToTech(CregTech creg_tech);
		static CregTech techToCreg(NetworkTech tech);
		
		virtual void handleCesq(const std::string &event);
		virtual void handleCsq(const std::string &event);
		virtual void handleCreg(const std::string &event);
		virtual void handleNetworkChange();
		
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
		bool m_prefer_sms_to_sim = false;
		bool m_storages_loaded = false;
		std::vector<SmsStorage> m_sms_all_storages[3];
		SmsStorage m_sms_mem[3] = {SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN};
		SmsStorageCapacity m_sms_capacity[3] = {};
		
		bool intiSms();
		bool syncSmsStorage();
		bool syncSmsCapacity();
		bool isSmsStorageSupported(int mem_id, SmsStorage check_storage);
		
		static std::string getSmsStorageName(SmsStorage storage);
		static Modem::SmsStorage getSmsStorageId(const std::string &name);
		static bool decodeSmsToPdu(const std::string &data, SmsDir *dir, Pdu *pdu, int *id, uint32_t *hash);
		
		bool discoverSmsStorages();
		bool findBestSmsStorage(bool prefer_sim);
	
	public:
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
		virtual std::vector<Operator> searchOperators() override;
		virtual bool setOperator(int mcc, int mnc, NetworkTech tech) override;
		
		virtual std::vector<NetworkModeItem> getNetworkModes() override;
		virtual bool setNetworkMode(int mode_id) override;
		
		virtual bool isRoamingEnabled() override;
		virtual bool setDataRoaming(bool enable) override;
		
		virtual std::tuple<bool, Operator> getCurrentOperator() override;
		
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
		virtual std::tuple<bool, std::vector<Sms>> getSmsList(SmsDir from_dir) override;
		virtual bool deleteSms(int id) override;
		virtual SmsStorageCapacity getSmsCapacity() override;
		virtual SmsStorage getSmsStorage() override;
		
		/*
		 * Internals
		 * */
		virtual IfaceProto getIfaceProto() override;
		virtual int getDelayAfterDhcpRelease() override;
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout) override;
		
		/*
		 * Modem configuration
		 * */
		virtual bool setOption(const std::string &name, const std::any &value) override;
};
