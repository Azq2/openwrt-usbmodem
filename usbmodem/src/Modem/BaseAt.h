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
		
		// Modem events handlers
		virtual void handleCesq(const std::string &event);
		virtual void handleCusd(const std::string &event);
		
		virtual void handleUssdResponse(int code, const std::string &data, int dcs);
		
		virtual bool ping(int tries);
		virtual bool handshake();
		
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
		
		// AT commands API
		virtual std::pair<bool, std::string> sendAtCommand(const std::string &cmd, int timeout = 0) override;
		
		// USSD API
		virtual bool sendUssd(const std::string &cmd, UssdCallback callback, int timeout = 0) override;
		virtual bool cancelUssd() override;
		virtual bool isUssdBusy() override;
		virtual bool isUssdWaitReply() override;
		
		// SMS API
		virtual bool decodeSmsToPdu(const std::string &data, SmsDir *dir, Pdu *pdu, int *id);
		virtual void getSmsList(SmsDir dir, SmsReadCallback callback) override;
};
