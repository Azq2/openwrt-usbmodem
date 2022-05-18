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

void Asr1802Modem::handleNetworkChange() {
	NetworkTech new_tech;
	NetworkReg new_net_reg;
	
	bool is_registered = false;
	if (m_cereg.isRegistered()) {
		new_tech = m_cereg.toNetworkTech();
		new_net_reg = m_cereg.toNetworkReg();
		is_registered = true;
	} else if (m_cgreg.isRegistered()) {
		new_tech = m_cgreg.toNetworkTech();
		new_net_reg = m_cgreg.toNetworkReg();
		is_registered = true;
	} else {
		new_tech = TECH_NO_SERVICE;
		new_net_reg = m_creg.toNetworkReg();
	}
	
	if (m_net_reg != new_net_reg) {
		m_net_reg = new_net_reg;
		emit<EvNetworkChanged>({.status = m_net_reg});
		
		Loop::setTimeout([this]() {
		//	handleOperatorChange();
		}, 0);
	}
	
	if (m_tech != new_tech) {
		m_tech = new_tech;
		emit<EvTechChanged>({.tech = m_tech});
	}
}
