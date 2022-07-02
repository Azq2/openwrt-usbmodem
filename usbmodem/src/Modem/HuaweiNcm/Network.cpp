#include "../HuaweiNcm.h"
#include <Core/Loop.h>
#include <cinttypes>
#include <unistd.h>

std::map<HuaweiNcmModem::NetworkMode, std::string> HuaweiNcmModem::m_mode2id = {
	{HuaweiNcmModem::NET_MODE_AUTO, "00"},
	
	{HuaweiNcmModem::NET_MODE_ONLY_2G, "01"},
	{HuaweiNcmModem::NET_MODE_ONLY_3G, "02"},
	{HuaweiNcmModem::NET_MODE_ONLY_4G, "03"},
	
	{HuaweiNcmModem::NET_MODE_PREFER_2G, "010302"},
	{HuaweiNcmModem::NET_MODE_PREFER_3G, "020301"},
	{HuaweiNcmModem::NET_MODE_PREFER_4G, "030201"},
	
	{HuaweiNcmModem::NET_MODE_2G_3G_PREFER_2G, "0102"},
	{HuaweiNcmModem::NET_MODE_2G_3G_PREFER_3G, "0201"},
	
	{HuaweiNcmModem::NET_MODE_2G_4G_PREFER_2G, "0103"},
	{HuaweiNcmModem::NET_MODE_2G_4G_PREFER_4G, "0301"},
	
	{HuaweiNcmModem::NET_MODE_3G_4G_PREFER_3G, "0203"},
	{HuaweiNcmModem::NET_MODE_3G_4G_PREFER_4G, "0302"},
};

void HuaweiNcmModem::handleHcsq(const std::string &event) {
	std::string sysmode;
	AtParser parser(event);
	
	m_signal.rssi_dbm = NAN;
	m_signal.rscp_dbm = NAN;
	m_signal.ecio_db = NAN;
	m_signal.rsrp_dbm = NAN;
	m_signal.sinr_db = NAN;
	m_signal.rsrq_db = NAN;
	
	if (!parser.parseNextString(&sysmode))
		return;
	
	if (sysmode != "NO SERVICE" && sysmode != "NOSERVICE") {
		int rssi;
		if (!parser.parseNextInt(&rssi))
			return;
		
		m_signal.rssi_dbm = decodeSignal(rssi, 121, 1, 255);
	}
	
	if (sysmode == "WCDMA" || sysmode == "TD-SCDMA") {
		int rscp, ecio;
		if (!parser.parseNextInt(&rscp))
			return;
		if (!parser.parseNextInt(&ecio))
			return;
		
		m_signal.rscp_dbm = decodeSignal(rscp, 121, 1, 255);
		m_signal.ecio_db = decodeSignal(ecio, 33, 0.5, 255);
	}

	if (sysmode == "LTE") {
		int rsrp, sinr, rsrq;
		if (!parser.parseNextInt(&rsrp))
			return;
		if (!parser.parseNextInt(&sinr))
			return;
		if (!parser.parseNextInt(&rsrq))
			return;
		
		m_signal.rsrp_dbm = decodeSignal(rsrp, 141, 1, 255);
		m_signal.sinr_db = decodeSignal(sinr, 21, 0.2, 255);
		m_signal.rsrq_db = decodeSignal(rsrq, 20, 0.5, 255);
	}
}

std::tuple<bool, bool> HuaweiNcmModem::isRoamingEnabled() {
	auto response = m_at.sendCommand("AT^SYSCFGEX?", "^SYSCFGEX");
	if (response.error)
		return {false, false};
	
	int roam;
	bool success = AtParser(response.data())
		.parseSkip()
		.parseSkip()
		.parseInt(&roam) // roam
		.success();
	if (!success)
		return {false, false};
	
	return {true, roam != 0};
}

bool HuaweiNcmModem::setDataRoaming(bool enable) {
	auto response = m_at.sendCommand("AT^SYSCFGEX?", "^SYSCFGEX");
	if (response.error)
		return false;
	
	std::string acqorder;
	int roam, srvdomain;
	uint64_t lteband, band;
	
	bool success = AtParser(response.data())
		.parseString(&acqorder) // acqorder
		.parseUInt64(&band, 16) // band
		.parseInt(&roam) // roam
		.parseInt(&srvdomain) // srvdomain
		.parseUInt64(&lteband, 16) // lteband
		.success();
	if (!success)
		return false;
	
	roam = enable ? 1 : 0;
	
	std::string query = strprintf("AT^SYSCFGEX=\"%s\",%" PRIX64 ",%d,%d,%" PRIX64 ",,", acqorder.c_str(), band, roam, srvdomain, lteband);
	return m_at.sendCommandNoResponse(query) == 0;
}

std::tuple<bool, HuaweiNcmModem::NetworkMode> HuaweiNcmModem::getCurrentNetworkMode() {
	auto response = m_at.sendCommand("AT^SYSCFGEX?", "^SYSCFGEX");
	if (response.error)
		return {false, NET_MODE_UNKNOWN};
	
	std::string acqorder;
	if (!AtParser(response.data()).parseNextString(&acqorder))
		return {false, NET_MODE_UNKNOWN};
	
	for (auto &it: m_mode2id) {
		if (it.second == acqorder)
			return {true, it.first};
	}
	
	return {false, NET_MODE_UNKNOWN};
}

std::tuple<bool, std::vector<HuaweiNcmModem::NetworkMode>> HuaweiNcmModem::getNetworkModes() {
	return {true, {
		NET_MODE_AUTO,
			
		NET_MODE_ONLY_2G,
		NET_MODE_ONLY_3G,
		NET_MODE_ONLY_4G,
		
		NET_MODE_PREFER_2G,
		NET_MODE_PREFER_3G,
		NET_MODE_PREFER_4G,
		
		NET_MODE_2G_3G_PREFER_2G,
		NET_MODE_2G_3G_PREFER_3G,
		
		NET_MODE_2G_4G_PREFER_2G,
		NET_MODE_2G_4G_PREFER_4G,
		
		NET_MODE_3G_4G_PREFER_3G,
		NET_MODE_3G_4G_PREFER_4G,
	}};
}

bool HuaweiNcmModem::setNetworkMode(NetworkMode new_mode) {
	if (m_mode2id.find(new_mode) == m_mode2id.end())
		return false;
	
	auto response = m_at.sendCommand("AT^SYSCFGEX?", "^SYSCFGEX");
	if (response.error)
		return false;
	
	std::string acqorder;
	int roam, srvdomain;
	uint64_t lteband, band;
	
	bool success = AtParser(response.data())
		.parseString(&acqorder) // acqorder
		.parseUInt64(&band, 16) // band
		.parseInt(&roam) // roam
		.parseInt(&srvdomain) // srvdomain
		.parseUInt64(&lteband, 16) // lteband
		.success();
	if (!success)
		return false;
	
	acqorder = m_mode2id[new_mode];
	
	std::string query = strprintf("AT^SYSCFGEX=\"%s\",%" PRIX64 ",%d,%d,%" PRIX64 ",,", acqorder.c_str(), band, roam, srvdomain, lteband);
	return m_at.sendCommandNoResponse(query) == 0;
}

std::tuple<bool, std::vector<HuaweiNcmModem::NetworkNeighborCell>> HuaweiNcmModem::getNeighboringCell() {
	return {true, {}};
}

std::tuple<bool, std::vector<HuaweiNcmModem::Operator>> HuaweiNcmModem::searchOperators() {
	m_at.sendCommandNoResponse("AT+CGATT=0");
	handleDisconnect();
	return BaseAtModem::searchOperators();
}

bool HuaweiNcmModem::setOperator(OperatorRegMode mode, int mcc, int mnc, NetworkTech tech) {
	// m_at.sendCommandNoResponse("AT+CGATT=0");
	return BaseAtModem::setOperator(mode, mcc, mnc, tech);
}
