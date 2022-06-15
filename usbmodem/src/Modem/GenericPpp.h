#pragma once

#include <vector>
#include <string>
#include <tuple>

#include "BaseAt.h"

class GenericPppModem: public BaseAtModem {
	protected:
		static constexpr int SIGNAL_INFO_UPDATE_IDLE_TIMEOUT	= 15000;
		static constexpr int SIGNAL_INFO_UPDATE_INTERVAL		= 2000;
		static constexpr int SIGNAL_INFO_UPDATE_INTERVAL_IDLE	= 15000;
		
		enum ModemNetType {
			MODEM_NET_GSM,
			MODEM_NET_CDMA
		};
		
		/*
		 * Internals
		 * */
		bool init() override;
		bool setOption(const std::string &name, const std::any &value) override;
		int getCommandTimeout(const std::string &cmd) override;
		
		/*
		 * Vendor-specific features
		 * */
		bool support_zrssi = false;
		bool support_zsnt = false;
		bool support_cesq = false;
		bool support_ussd = false;
		bool support_sms = false;
		
		/*
		 * Network
		 * */
		int m_signal_recheck_timeout = -1;
		int64_t m_signal_last_requested = 0;
		static std::map<NetworkMode, int> m_zte_mode2id;
		
		void requestSignalInfo();
		void startSignalPolling();
		void wakeSignalTimer();
		
		void handleZrssi(const std::string &event);
		
		std::tuple<bool, std::vector<NetworkMode>> getNetworkModes() override;
		bool setNetworkMode(NetworkMode new_mode) override;
		std::tuple<bool, NetworkMode> getCurrentNetworkMode() override;
		
		/*
		 * Vendor
		 * */
		enum Vendor {
			VENDOR_UNKNOWN,
			VENDOR_ZTE,
			VENDOR_HUAWEI
		};
		
		ModemNetType m_net_type = MODEM_NET_GSM;
		Vendor m_vendor = VENDOR_UNKNOWN;
		Vendor getModemVendor();
	public:
		GenericPppModem() : BaseAtModem() { }
		
		/*
		 * Network
		 * */
		std::tuple<bool, NetworkInfo> getNetworkInfo() override;
		 
		/*
		 * Internals
		 * */
		std::vector<Capability> getCapabilities() override;
		
		bool close() override;
		
		virtual ~GenericPppModem() { }
};
