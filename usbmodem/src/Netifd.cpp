#include "Netifd.h"

Netifd::Netifd() {
	
}

bool Netifd::createDynamicIface(const std::string &proto, const std::string &iface, const std::string &parent_iface,
	const std::string &fw_zone, const std::map<std::string, std::string> &default_options)
{
	json_object *request = json_object_new_object();
	
	if (!request)
		return false;
	
	std::string ifname = "@" + parent_iface;
	json_object_object_add(request, "name", json_object_new_string(iface.c_str()));
	json_object_object_add(request, "ifname", json_object_new_string(ifname.c_str()));
	json_object_object_add(request, "proto", json_object_new_string(proto.c_str()));
	
	if (proto == "dhcpv6")
		json_object_object_add(request, "extendprefix", json_object_new_int(1));
	
	if (fw_zone.size() > 0)
		json_object_object_add(request, "zone", json_object_new_string(fw_zone.c_str()));
	
	// proto_add_dynamic_defaults()
	const char *options[] = {"defaultroute", "peerdns", "metric", nullptr};
	const char **option_it = options;
	while (*option_it) {
		auto it = default_options.find(*option_it);
		if (it != default_options.end())
			json_object_object_add(request, it->first.c_str(), json_object_new_string(it->second.c_str()));
		option_it++;
	}
	
	bool result = m_ubus->call("network", "add_dynamic", request);
	json_object_put(request);
	return result;
}

json_object *Netifd::newIpAddItem(const std::string &ipaddr, const std::string &mask, const std::string &broadcast) {
	json_object *item = json_object_new_object();
	if (!item)
		return nullptr;
	
	json_object_object_add(item, "ipaddr", json_object_new_string(ipaddr.c_str()));
	
	if (mask.size() > 0)
		json_object_object_add(item, "mask", json_object_new_string(mask.c_str()));
	
	if (broadcast.size() > 0)
		json_object_object_add(item, "broadcast", json_object_new_string(broadcast.c_str()));
	
	return item;
}

json_object *Netifd::newRouteItem(const std::string &target, const std::string &mask, const std::string &gateway) {
	json_object *item = json_object_new_object();
	if (!item)
		return nullptr;
	
	json_object_object_add(item, "target", json_object_new_string(target.c_str()));
	json_object_object_add(item, "netmask", json_object_new_string(mask.c_str()));
	
	if (gateway.size() > 0)
		json_object_object_add(item, "gateway", json_object_new_string(gateway.c_str()));
	
	return item;
}

bool Netifd::updateIface(const std::string &iface, const std::string &ifname, const IpInfo *ipv4, const IpInfo *ipv6) {
	json_object *request = json_object_new_object();
	
	if (!request)
		return false;
	
	json_object_object_add(request, "action", json_object_new_int(0));
	json_object_object_add(request, "ifname", json_object_new_string(ifname.c_str()));
	json_object_object_add(request, "interface", json_object_new_string(iface.c_str()));
	json_object_object_add(request, "link-up", json_object_new_boolean(true));
	json_object_object_add(request, "address-external", json_object_new_boolean(false));
	json_object_object_add(request, "keep", json_object_new_boolean(true));
	
	json_object *ipaddr = json_object_new_array();
	json_object_object_add(request, "ipaddr", ipaddr);
	
	json_object *ip6addr = json_object_new_array();
	json_object_object_add(request, "ip6addr", ip6addr);
	
	json_object *routes = json_object_new_array();
	json_object_object_add(request, "routes", routes);
	
	json_object *routes6 = json_object_new_array();
	json_object_object_add(request, "routes6", routes6);
	
	json_object *dns = json_object_new_array();
	json_object_object_add(request, "dns", dns);
	
	if (!ipaddr || !ip6addr || !routes || !routes6 || !dns) {
		json_object_put(request);
		return false;
	}
	
	json_object *ipaddr_item, *route_item;
	
	if (ipv4 && ipv4->ip.size() > 0) {
		// IPv4 addr
		ipaddr_item = newIpAddItem(ipv4->ip, ipv4->mask);
		if (!ipaddr_item) {
			json_object_put(request);
			return false;
		}
		json_object_array_add(ipaddr, ipaddr_item);
		
		// IPv4 Routes
		route_item = newRouteItem("0.0.0.0", "0", ipv4->gw);
		if (!route_item) {
			json_object_put(request);
			return false;
		}
		json_object_array_add(routes, route_item);
		
		// IPv4 DNS
		if (ipv4->dns1.size() > 0)
			json_object_array_add(dns, json_object_new_string(ipv4->dns1.c_str()));
		
		if (ipv4->dns2.size() > 0 && ipv4->dns1 != ipv4->dns2)
			json_object_array_add(dns, json_object_new_string(ipv4->dns2.c_str()));
	}
	
	if (ipv6 && ipv6->ip.size() > 0) {
		// IPv6 addr
		ipaddr_item = newIpAddItem(ipv6->ip, ipv6->mask);
		if (!ipaddr_item) {
			json_object_put(request);
			return false;
		}
		json_object_array_add(ipaddr, ipaddr_item);
		
		// IPv6 Routes
		route_item = newRouteItem("::", "0", ipv6->gw);
		if (!route_item) {
			json_object_put(request);
			return false;
		}
		json_object_array_add(routes, route_item);
		
		// IPv6 DNS
		if (ipv6->dns1.size() > 0)
			json_object_array_add(dns, json_object_new_string(ipv6->dns1.c_str()));
		
		if (ipv6->dns2.size() > 0 && ipv6->dns1 != ipv6->dns2)
			json_object_array_add(dns, json_object_new_string(ipv6->dns2.c_str()));
	}
	
	bool result = m_ubus->call("network.interface", "notify_proto", request);
	json_object_put(request);
	return result;
}

bool Netifd::protoKill(const std::string &iface, int signal) {
	json_object *request = json_object_new_object();
	
	if (!request)
		return false;
	
	LOGD("Send signal %d to '%s'\n", signal, iface.c_str());
	
	json_object_object_add(request, "action", json_object_new_int(2));
	json_object_object_add(request, "signal", json_object_new_int(signal));
	json_object_object_add(request, "interface", json_object_new_string(iface.c_str()));
	
	bool result = m_ubus->call("network.interface", "notify_proto", request);
	json_object_put(request);
	return result;
}
