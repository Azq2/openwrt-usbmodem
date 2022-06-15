#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "BaseAt.h"

class Asr1802Modem: public BaseAtModem {
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
		int m_connect_errors = 0;
		bool m_prefer_dhcp = false;
		bool m_force_restart_network = false;
		bool m_is_td_modem = false; // true - TDSCDMA, false - WCDMA
		int m_pdp_context = DEFAULT_PDP_CONTEXT;
		std::vector<NetworkNeighborCell> m_neighboring_cell;
		static std::map<NetworkMode, int> m_mode2id;
		
		bool dial();
		bool syncApn();
		void startDataConnection();
		int getCurrentPdpCid();
		void restartNetwork();
		
		std::tuple<bool, NetworkInfo> getNetworkInfo() override;
		IfaceProto getIfaceProto() override;
		int getDelayAfterDhcpRelease() override;
		
		void handleCgev(const std::string &event);
		void handleCesq(const std::string &event) override;
		void handleMmsg(const std::string &event);
		void handleServingCell(const std::string &event);
		void handleNeighboringCell(const std::string &event);
		void handleEngInfoStart(const std::string &event);
		
		std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() override;
		bool setNetworkMode(NetworkMode new_mode) override;
		std::tuple<bool, NetworkMode> getCurrentNetworkMode() override;
		
		std::tuple<bool, bool> isRoamingEnabled() override;
		bool setDataRoaming(bool enable) override;
		
		std::tuple<bool, std::vector<NetworkNeighborCell>> getNeighboringCell() override;
		
		void handleConnect();
		void handleDisconnect();
		void handleConnectError();
		
		bool detectModemType();
		
		/*
		 * Engineering Info
		 * */
		int m_eng_recheck_timeout = -1;
		int64_t m_eng_last_requested = 0;
		
		void requestEngInfo();
		void startEngPolling();
		void wakeEngTimer();
		
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
		
		virtual ~Asr1802Modem() { }
};
