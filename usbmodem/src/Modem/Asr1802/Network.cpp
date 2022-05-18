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

void Asr1802Modem::handleCesq(const std::string &event) {
	BaseAtModem::handleCesq(event);
	
	if (std::isnan(m_signal.rssi_dbm) && !std::isnan(m_signal.rscp_dbm)) {
		m_signal.rssi_dbm = m_signal.rscp_dbm;
		m_signal.rscp_dbm = NAN;
	}
}
