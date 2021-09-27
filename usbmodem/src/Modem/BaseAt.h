#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <map>

#include "../Modem.h"
#include "../Serial.h"
#include "../AtChannel.h"
#include "../AtParser.h"
#include "../GsmUtils.h"

/*
 * Base driver for any AT modem
 * */
class ModemBaseAt: public Modem {
	protected:
		Serial m_serial;
		AtChannel m_at;
		
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
		
		Creg m_creg = {};
		Creg m_cereg = {};
		Creg m_cgreg = {};
		
		bool m_self_test = false;
		
		int m_connect_timeout = 0;
		int m_connect_timeout_id = -1;
		
		bool m_pincode_entered = false;
		
		uint32_t m_ussd_request_id = 0;
		int m_ussd_timeout = -1;
		bool m_ussd_session = false;
		UssdCallback m_ussd_callback;
		
		bool m_sms_ready = false;
		bool m_prefer_sms_to_sim = false;
		bool m_storages_loaded = false;
		std::vector<SmsStorage> m_sms_all_storages[3];
		
		SmsStorage m_sms_mem[3] = {SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN, SMS_STORAGE_UNKNOWN};
		SmsStorageCapacity m_sms_capacity[3] = {};
		
		// Modem events handlers
		virtual void handleCesq(const std::string &event);
		virtual void handleCusd(const std::string &event);
		
		virtual void handleUssdResponse(int code, const std::string &data, int dcs);
		
		virtual bool ping(int tries);
		virtual bool handshake();
		
		/*
		 * SMS
		 * */
		virtual bool intiSms();
		virtual SmsStorage getSmsStorageId(const std::string &name);
		virtual std::string getSmsStorageName(SmsStorage storage);
		virtual bool findBestSmsStorage(bool prefer_sim);
		virtual bool discoverSmsStorages();
		virtual bool isSmsStorageSupported(int mem_id, SmsStorage check_storage);
		virtual bool decodeSmsToPdu(const std::string &data, SmsDir *dir, Pdu *pdu, int *id, uint32_t *hash);
		virtual bool syncSmsCapacity();
		virtual bool syncSmsStorage();
		
		/*
		 * SIM PIN
		 * */
		virtual void startSimPolling();
		virtual void handleCpin(const std::string &event);
		
		/*
		 * Network watchdog
		 * */
		void startNetRegWhatchdog();
		void stopNetRegWhatchdog();
		
		/*
		 * Modem identification
		 * */
		virtual bool readModemIdentification();
		
		/*
		 * SIM identification
		 * */
		virtual bool readSimIdentification();
		
		/*
		 * Internal
		 * */
		virtual int getCommandTimeout(const std::string &cmd);
	public:
		ModemBaseAt();
		virtual ~ModemBaseAt();
		
		virtual bool init() = 0;
		virtual bool open() override;
		virtual void close() override;
		virtual bool setCustomOption(const std::string &name, const std::any &value) override;
		
		/*
		 * Command API
		 */
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout = 0) override;
		
		/*
		 * USSD API
		 */
		virtual bool sendUssd(const std::string &cmd, UssdCallback callback, int timeout = 0) override;
		virtual bool cancelUssd() override;
		virtual bool isUssdBusy() override;
		virtual bool isUssdWaitReply() override;
		
		/*
		 * SMS API
		 */
		virtual void getSmsList(SmsDir from_dir, SmsReadCallback callback) override;
		virtual bool deleteSms(int id) override;
		virtual SmsStorageCapacity getSmsCapacity() override;
		virtual SmsStorage getSmsStorage() override;
};
