#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "BaseAt.h"

class ModemAsr1802: public ModemBaseAt {
	protected:
		static constexpr int DEFAULT_PDP_CONTEXT = 5;
		
		enum DataConnectState {
			DISCONNECTED		= 0,
			CONNECTING			= 1,
			CONNECTED			= 2
		};
		
		DataConnectState m_data_state = DISCONNECTED;
		int m_manual_connect_timeout = -1;
		int m_connect_errors = 0;
		bool m_prefer_dhcp = false;
		bool m_force_restart_network = false;
		
		int m_pdp_context = DEFAULT_PDP_CONTEXT;
		
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
		
		void handleUssdResponse(int code, const std::string &data, int dcs) override;
		
		// Manual connection
		bool dial();
		void startDataConnection();
		
		int getCurrentPdpCid();
		
		bool setRadioOn(bool state);
		bool isRadioOn();
		
		void restartNetwork();
		
		int getCommandTimeout(const std::string &cmd) override;
	public:
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		void finish() override;
		bool setCustomOption(const std::string &name, const std::any &value) override;
		
		ModemAsr1802();
		~ModemAsr1802();
};
