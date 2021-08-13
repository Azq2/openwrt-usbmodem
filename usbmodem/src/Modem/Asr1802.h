#pragma once

#include <vector>
#include <string>
#include <thread>

#include "../Modem.h"

class ModemAsr1802: public Modem {
	protected:
		static constexpr int DEFAULT_PDP_CONTEXT = 5;
		
		enum Timeouts {
			TIMEOUT_CFUN		= 50 * 1000,
			TIMEOUT_CGDATA		= 185 * 1000,
			TIMEOUT_CUSD		= 110 * 1000,
			TIMEOUT_CPIN		= 185 * 1000
		};
		
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
			
			inline bool isRegistered() const {
				switch (status) {
					case CREG_REGISTERED_HOME:				return true;
					case CREG_REGISTERED_ROAMING:			return true;
					case CREG_REGISTERED_HOME_NO_CSFB:		return true;
					case CREG_REGISTERED_ROAMING_NO_CSFB:	return true;
				}
				return false;
			}
			
			inline NetworkTech toNetworkTech() const {
				if (!isRegistered())
					return TECH_NO_SERVICE;
				
				switch (tech) {
					case CREG_TECH_GSM:				return TECH_GSM;
					case CREG_TECH_GSM_COMPACT:		return TECH_GSM;
					case CREG_TECH_UMTS:			return TECH_UMTS;
					case CREG_TECH_EDGE:			return TECH_EDGE;
					case CREG_TECH_HSDPA:			return TECH_HSDPA;
					case CREG_TECH_HSUPA:			return TECH_HSUPA;
					case CREG_TECH_HSPA:			return TECH_HSPA;
					case CREG_TECH_HSPAP:			return TECH_HSPAP;
					case CREG_TECH_LTE:				return TECH_LTE;
				}
				return TECH_UNKNOWN;
			}
			
			inline NetworkReg toNetworkReg() const {
				switch (status) {
					case CREG_NOT_REGISTERED:					return NET_NOT_REGISTERED;
					case CREG_REGISTERED_HOME:					return NET_REGISTERED_HOME;
					case CREG_SEARCHING:						return NET_SEARCHING;
					case CREG_REGISTRATION_DENIED:				return NET_NOT_REGISTERED;
					case CREG_UNKNOWN:							return NET_NOT_REGISTERED;
					case CREG_REGISTERED_ROAMING:				return NET_REGISTERED_ROAMING;
					case CREG_REGISTERED_HOME_SMS_ONLY:			return NET_REGISTERED_HOME;
					case CREG_REGISTERED_ROAMING_SMS_ONLY:		return NET_REGISTERED_ROAMING;
					case CREG_REGISTERED_EMERGENY_ONLY:			return NET_NOT_REGISTERED;
					case CREG_REGISTERED_HOME_NO_CSFB:			return NET_REGISTERED_HOME;
					case CREG_REGISTERED_ROAMING_NO_CSFB:		return NET_REGISTERED_ROAMING;
					case CREG_REGISTERED_EMERGENY_ONLY2:		return NET_NOT_REGISTERED;
				}
				return NET_NOT_REGISTERED;
			}
		};
		
		enum DataConnectState {
			DISCONNECTED		= 0,
			CONNECTING			= 1,
			CONNECTED			= 2
		};
		
		// CS status
		Creg m_creg = {};
		
		// GPRS / 3G status
		Creg m_cereg = {};
		
		// LTE status
		Creg m_cgreg = {};
		
		DataConnectState m_data_state = DISCONNECTED;
		int m_data_connect_timeout = -1;
		int m_connect_errors = 0;
		
		int m_pdp_context = DEFAULT_PDP_CONTEXT;
		
		int getDefaultAtTimeout() override;
		
		bool init() override;
		bool initDefaults();
		bool syncApn();
		
		void handleConnect();
		void handleDisconnect();
		void handleConnectError();
		
		// Modem events handlers
		void handleCreg(const std::string &event);
		void handleCgev(const std::string &event);
		void handleNetworkChange();
		
		// Manual connection
		bool dial();
		void startDataConnection();
		
		int getCurrentPdpCid();
		
		bool setRadioOn(bool state);
		bool isRadioOn();
		
		void restartNetwork();
	public:
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		
		ModemAsr1802();
		~ModemAsr1802();
};
