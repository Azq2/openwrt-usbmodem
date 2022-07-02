#include "ModemService.h"

#include <vector>
#include <csignal>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

bool ModemService::startDhcp() {
	if (m_dhcp_inited) {
		if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_4")) {
				LOGE("Can't send dhcp renew for '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.dhcpRenew(m_iface + "_6")) {
				LOGE("Can't send dhcp renew for '%s_6'\n", m_iface.c_str());
				return false;
			}
		}
	} else {
		if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcp", m_iface + "_4", m_iface, m_fw_zone, m_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
			if (!m_netifd.createDynamicIface("dhcpv6", m_iface + "_4", m_iface, m_fw_zone, m_options)) {
				LOGE("Can't create dhcp interface '%s_4'\n", m_iface.c_str());
				return false;
			}
		}
		
		m_dhcp_inited = true;
	}
	return true;
}

bool ModemService::stopDhcp() {
	if (!m_dhcp_inited)
		return true;
	
	if (m_options["pdp_type"] == "IP" || m_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_4")) {
			LOGE("Can't send dhcp release for '%s_4'\n", m_iface.c_str());
			return false;
		}
	}
	
	if (m_options["pdp_type"] == "IPV6" || m_options["pdp_type"] == "IPV4V6") {
		if (!m_netifd.dhcpRelease(m_iface + "_6")) {
			LOGE("Can't send dhcp release for '%s_6'\n", m_iface.c_str());
			return false;
		}
	}
	
	return true;
}
