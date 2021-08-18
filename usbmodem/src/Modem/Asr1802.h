#pragma once

#include <vector>
#include <string>

#include "../Modem.h"

class ModemAsr1802: public Modem {
	protected:
		static constexpr int DEFAULT_PDP_CONTEXT = 5;
		
		enum Timeouts {
			TIMEOUT_CFUN		= 50 * 1000,
			TIMEOUT_CGDATA		= 185 * 1000,
			TIMEOUT_CUSD		= 110 * 1000,
			TIMEOUT_CGATT		= 110 * 1000,
			TIMEOUT_CPIN		= 185 * 1000
		};
		
		enum DataConnectState {
			DISCONNECTED		= 0,
			CONNECTING			= 1,
			CONNECTED			= 2
		};
		
		DataConnectState m_data_state = DISCONNECTED;
		int m_manual_connect_timeout = -1;
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
		void handleCgev(const std::string &event);
		void handleCreg(const std::string &event);
		void handleNetworkChange();
		
		// Manual connection
		bool dial();
		void startDataConnection();
		
		void startSimPolling();
		
		int getCurrentPdpCid();
		
		bool setRadioOn(bool state);
		bool isRadioOn();
		
		void restartNetwork();
	public:
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		void finish() override;
		
		ModemAsr1802();
		~ModemAsr1802();
};
