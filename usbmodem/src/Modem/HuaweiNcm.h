#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "BaseAt.h"

class HuaweiNcmModem: public BaseAtModem {
	protected:
		static constexpr int DEFAULT_PDP_CONTEXT		= 5;
		static constexpr int ENG_INFO_UPDATE_TIMEOUT	= 15000;
		
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
		std::vector<Capability> getCapabilities() override;
		
		/*
		 * Network
		 * */
		DataConnectState m_data_state = DISCONNECTED;
		int m_manual_connect_timeout = -1;
		bool m_prefer_dhcp = false;
		bool m_first_data_connect = true;
		int m_pdp_context = DEFAULT_PDP_CONTEXT;
		static std::map<NetworkMode, std::string> m_mode2id;
		
		bool dial();
		bool syncApn();
		void startDataConnection();
		
		bool readDhcpV4();
		bool readDhcpV6();
		
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		
		void handleCgev(const std::string &event);
		void handleHcsq(const std::string &event);
		
		std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() override;
		bool setNetworkMode(NetworkMode new_mode) override;
		std::tuple<bool, NetworkMode> getCurrentNetworkMode() override;
		
		std::tuple<bool, bool> isRoamingEnabled() override;
		bool setDataRoaming(bool enable) override;
		
		std::tuple<bool, std::vector<NetworkNeighborCell>> getNeighboringCell() override;
		
		void handleConnect();
		void handleDisconnect();
		void handleConnectError();
		
		/*
		 * Maintenance
		 * */
		bool setRadioOn(bool state);
		bool isRadioOn();
	public:
		HuaweiNcmModem() : BaseAtModem() { }
		
		bool close() override;
		
		virtual ~HuaweiNcmModem() { }
};
