#pragma once
#pragma once

#include "Log.h"
#include "Ubus.h"
#include "Modem.h"

#include <signal.h>

class Netifd {
	protected:
		Ubus *m_ubus = nullptr;
	
	public:
		Netifd();
		
		inline void setUbus(Ubus *ubus) {
			m_ubus = ubus;
		}
		
		inline bool avail() const {
			return m_ubus != nullptr && m_ubus->avail();
		}
		
		// Block iface restart
		bool protoBlockRestart(const std::string &iface);
		
		// Update static iface
		bool updateIface(const std::string &iface, const std::string &ifname, const IpInfo *ipv4, const IpInfo *ipv6);
		
		// Create dynamic interface
		bool createDynamicIface(const std::string &proto, const std::string &iface, const std::string &parent_iface,
			const std::string &fw_zone, const std::map<std::string, std::string> &default_options);
		
		// Send signal to iface
		bool protoKill(const std::string &iface, int signal);
		
		// Send error to iface
		bool protoError(const std::string &iface, const std::string &error);
		
		// Set "available" flag for iface
		bool protoSetAvail(const std::string &iface, bool avail);
		
		// Renew DHCP
		inline bool dhcpRenew(const std::string &iface) {
			return protoKill(iface, SIGUSR1);
		}
		
		// Renew DHCP
		inline bool dhcpRelease(const std::string &iface) {
			return protoKill(iface, SIGUSR2);
		}
};
