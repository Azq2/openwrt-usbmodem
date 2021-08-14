#pragma once
#pragma once

#include "Log.h"
#include "Ubus.h"
#include "Modem.h"

#include <signal.h>
#include <json-c/json.h>

class Netifd {
	protected:
		Ubus *m_ubus = nullptr;
	
	public:
		Netifd();
		
		inline void setUbus(Ubus *ubus) {
			m_ubus = ubus;
		}
		
		// Update static iface
		bool updateIface(const std::string &iface, const std::string &ifname, const IpInfo *ipv4, const IpInfo *ipv6);
		
		// Create dynamic interface
		bool createDynamicIface(const std::string &proto, const std::string &iface, const std::string &parent_iface,
			const std::string &fw_zone, const std::map<std::string, std::string> &default_options);
		
		// Send signal to proto
		bool protoKill(const std::string &iface, int signal);
		
		// Renew DHCP
		inline bool dhcpRenew(const std::string &iface) {
			return protoKill(iface, SIGUSR1);
		}
		
		// Renew DHCP
		inline bool dhcpRelease(const std::string &iface) {
			return protoKill(iface, SIGUSR2);
		}
		
		json_object *newIpAddItem(const std::string &ipaddr, const std::string &mask = "", const std::string &broadcast = "");
		json_object *newRouteItem(const std::string &target, const std::string &mask, const std::string &gateway = "");
};
