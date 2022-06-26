#include "Netifd.h"

Netifd::Netifd() {
	
}

bool Netifd::protoBlockRestart(const std::string &iface) {
	return m_ubus->call("network.interface", "notify_proto", {
		{"action", 4},
		{"interface", iface}
	});
}

bool Netifd::protoSetAvail(const std::string &iface, bool avail) {
	return m_ubus->call("network.interface", "notify_proto", {
		{"action", 5},
		{"available", avail},
		{"interface", iface}
	});
}

bool Netifd::protoError(const std::string &iface, const std::string &error) {
	return m_ubus->call("network.interface", "notify_proto", {
		{"action", 3},
		{"error", {error}},
		{"interface", iface}
	});
}

bool Netifd::createDynamicIface(const std::string &proto, const std::string &iface, const std::string &parent_iface,
	const std::string &fw_zone, const std::map<std::string, std::string> &default_options)
{
	json request = {
		{"name", iface},
		{"ifname", "@" + parent_iface},
		{"proto", proto},
	};
	
	if (proto == "dhcpv6")
		request["extendprefix"] = 1;
	
	if (fw_zone.size() > 0)
		request["zone"] = fw_zone;
	
	// proto_add_dynamic_defaults()
	const char *options[] = {"defaultroute", "peerdns", "metric", nullptr};
	const char **option_it = options;
	while (*option_it) {
		auto it = default_options.find(*option_it);
		if (it != default_options.end())
			request[it->first] = it->second;
		option_it++;
	}
	
	return m_ubus->call("network", "add_dynamic", request);
}

bool Netifd::updateIface(const std::string &iface, const std::string &ifname, const Modem::IpInfo *ipv4, const Modem::IpInfo *ipv6) {
	json request = {
		{"action", 0},
		{"link-up", true},
		{"ifname", ifname},
		{"interface", iface},
		{"address-external", false},
		{"keep", false},
	};
	
	if (ipv4 && ipv4->ip.size() > 0) {
		// IPv4 addr
		json ipaddr_item = {{"ipaddr", ipv4->ip}};
		if (ipv4->mask.size() > 0)
			ipaddr_item["mask"] = ipv4->mask;
		request["ipaddr"].push_back(ipaddr_item);
		
		
		// IPv4 Routes
		json route_item = {{"target", "0.0.0.0"}, {"netmask", "0"}};
		if (ipv4->gw.size() > 0)
			route_item["gateway"] = ipv4->gw;
		request["routes"].push_back(route_item);
		
		// IPv4 DNS
		if (ipv4->dns1.size() > 0)
			request["dns"].push_back(ipv4->dns1);
		
		if (ipv4->dns2.size() > 0 && ipv4->dns1 != ipv4->dns2)
			request["dns"].push_back(ipv4->dns2);
	}
	
	if (ipv6 && ipv6->ip.size() > 0) {
		// IPv6 addr
		json ipaddr_item = {{"ipaddr", ipv6->ip}};
		if (ipv6->mask.size() > 0)
			ipaddr_item["mask"] = ipv6->mask;
		request["ip6addr"].push_back(ipaddr_item);
		
		
		// IPv6 Routes
		json route_item = {{"target", "::"}, {"netmask", "0"}};
		if (ipv6->gw.size() > 0)
			route_item["gateway"] = ipv6->gw;
		request["routes6"].push_back(route_item);
		
		// IPv6 DNS
		if (ipv6->dns1.size() > 0)
			request["dns"].push_back(ipv6->dns1);
		
		if (ipv6->dns2.size() > 0 && ipv6->dns1 != ipv6->dns2)
			request["dns"].push_back(ipv6->dns2);
	}
	
	LOGD("request=%s\n", request.dump(2).c_str());
	
	return m_ubus->call("network.interface", "notify_proto", request);
}

bool Netifd::protoKill(const std::string &iface, int signal) {
	return m_ubus->call("network.interface", "notify_proto", {
		{"action", 2},
		{"signal", signal},
		{"interface", iface}
	});
}
