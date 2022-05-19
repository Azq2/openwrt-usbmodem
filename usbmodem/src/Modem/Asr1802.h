#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "BaseAt.h"

class Asr1802Modem: public BaseAtModem {
	protected:
		static constexpr int DEFAULT_PDP_CONTEXT = 5;
		
		enum DataConnectState {
			DISCONNECTED		= 0,
			CONNECTING			= 1,
			CONNECTED			= 2
		};
	
	protected:
		/*
		 * Internals
		 * */
		bool init() override;
		bool setOption(const std::string &name, const std::any &value) override;
		int getCommandTimeout(const std::string &cmd) override;
		
		/*
		 * Network
		 * */
		DataConnectState m_data_state = DISCONNECTED;
		int m_manual_connect_timeout = -1;
		int m_connect_errors = 0;
		bool m_prefer_dhcp = false;
		bool m_force_restart_network = false;
		int m_pdp_context = DEFAULT_PDP_CONTEXT;
		
		bool dial();
		bool syncApn();
		void startDataConnection();
		int getCurrentPdpCid();
		void restartNetwork();
		
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		
		void handleCgev(const std::string &event);
		void handleCesq(const std::string &event) override;
		
		std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() override;
		bool setNetworkMode(NetworkMode new_mode) override;
		
		void handleConnect();
		void handleDisconnect();
		void handleConnectError();
		
		/*
		 * Maintenance
		 * */
		bool setRadioOn(bool state);
		bool isRadioOn();
		
		/*
		 * USSD
		 * */
		void handleUssdResponse(int code, const std::string &data, int dcs) override;
	public:
		Asr1802Modem() : BaseAtModem() { }
		
		bool close() override;
		
		~Asr1802Modem() { }
};
