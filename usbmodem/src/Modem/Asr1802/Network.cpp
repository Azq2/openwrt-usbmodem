#include "../Asr1802.h"
#include <Core/Loop.h>

void Asr1802Modem::handleCgev(const std::string &event) {
	// "DEACT" and "DETACH" mean disconnect
	if (event.find("DEACT") != std::string::npos || event.find("DETACH") != std::string::npos) {
		Loop::setTimeout([this]() {
			handleDisconnect();
		}, 0);
	}
	// Other events handle as "connection changed"
	else {
		// Ignore this event for 3G/EDGE
		if (m_tech == TECH_LTE) {
			Loop::setTimeout([this]() {
				handleConnect();
			}, 0);
		}
	}
}

std::tuple<bool, Asr1802Modem::NetworkInfo> Asr1802Modem::getNetworkInfo() {
	wakeEngTimer();
	return BaseAtModem::getNetworkInfo();
}

void Asr1802Modem::handleCesq(const std::string &event) {
	bool is_3g = (m_tech == TECH_UMTS || m_tech == TECH_HSDPA || m_tech == TECH_HSUPA || m_tech == TECH_HSPA || m_tech == TECH_HSPAP);
	if (is_3g || m_tech == TECH_LTE) {
		Loop::setTimeout([this]() {
			requestEngInfo();
		}, 0);
	} else {
		BaseAtModem::handleCesq(event);
	}
}

bool Asr1802Modem::detectModemType() {
	auto response = m_at.sendCommand("AT*EHSDPA=?", "*EHSDPA");
	if (response.error)
		return false;
	
	// As datasheet
	// TDSCDMA: (0-3),(1-11,13-16,23,35),(6),(0),(0),(0),(0),(0)
	// WCDMA: (0-2,4),(1-12),(1-6),(0,1),(1-14),(7),(0,1),(0,1)
	
	m_is_td_modem = !strcasecmp(response.data().c_str(), "(0-2,4),(1-12),");
	
	return true;
}

std::tuple<bool, std::vector<Asr1802Modem::NetworkMode>> Asr1802Modem::getNetworkModes() {
	return {true, {
		NET_MODE_AUTO,
			
		NET_MODE_ONLY_2G,
		NET_MODE_ONLY_3G,
		NET_MODE_ONLY_4G,
		
		NET_MODE_PREFER_2G,
		NET_MODE_PREFER_3G,
		NET_MODE_PREFER_4G,
		
		NET_MODE_2G_3G_AUTO,
		NET_MODE_2G_3G_PREFER_2G,
		NET_MODE_2G_3G_PREFER_3G,
		
		NET_MODE_2G_4G_AUTO,
		NET_MODE_2G_4G_PREFER_2G,
		NET_MODE_2G_4G_PREFER_4G,
		
		NET_MODE_3G_4G_AUTO,
		NET_MODE_3G_4G_PREFER_3G,
		NET_MODE_3G_4G_PREFER_4G,
	}};
}

bool Asr1802Modem::setNetworkMode(NetworkMode new_mode) {
	static std::map<NetworkMode, int> mode2id = {
		{NET_MODE_AUTO, 12},
		{NET_MODE_ONLY_2G, 0},
		{NET_MODE_ONLY_3G, 1},
		{NET_MODE_ONLY_4G, 5},
		{NET_MODE_PREFER_2G, 13},
		{NET_MODE_PREFER_3G, 14},
		{NET_MODE_PREFER_4G, 15},
		{NET_MODE_2G_3G_AUTO, 2},
		{NET_MODE_2G_3G_PREFER_2G, 3},
		{NET_MODE_2G_3G_PREFER_3G, 4},
		{NET_MODE_2G_4G_AUTO, 6},
		{NET_MODE_2G_4G_PREFER_2G, 7},
		{NET_MODE_2G_4G_PREFER_4G, 8},
		{NET_MODE_3G_4G_AUTO, 9},
		{NET_MODE_3G_4G_PREFER_3G, 10},
		{NET_MODE_3G_4G_PREFER_4G, 11},
	};
	
	if (mode2id.find(new_mode) == mode2id.end())
		return false;
	
	auto response = m_at.sendCommand("AT*BAND?", "*BAND");
	if (response.error)
		return false;
	
	int mode, gsm_band, umts_band, lte_bandh, lte_bandl, roaming_cfg, srv_domain, band_priority;
	bool success = AtParser(response.data())
		.parseInt(&mode) // mode
		.parseInt(&gsm_band) // GSMband
		.parseInt(&umts_band) // UMTSband
		.parseInt(&lte_bandh) // LTEbandH
		.parseInt(&lte_bandl) // LTEbandL
		.parseInt(&roaming_cfg) // roamingConfig
		.parseInt(&srv_domain) // srvDomain
		.parseInt(&band_priority) // bandPriorityFlag
		.success();
	if (!success)
		return false;
	
	mode = mode2id[new_mode];
	
	std::string query = strprintf("AT*BAND=%d,%d,%d,%d,%d,%d,%d,%d",
		mode, gsm_band, umts_band, lte_bandh, lte_bandl, roaming_cfg, srv_domain, band_priority);
	return m_at.sendCommandNoResponse(query) == 0;
}
