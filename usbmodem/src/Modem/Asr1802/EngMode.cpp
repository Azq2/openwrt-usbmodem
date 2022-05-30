#include "../Asr1802.h"
#include <Core/Loop.h>

void Asr1802Modem::wakeEngTimer() {
	if (getCurrentTimestamp() - m_eng_last_requested >= ENG_INFO_UPDATE_TIMEOUT) {
		m_eng_last_requested = getCurrentTimestamp();
		Loop::clearTimeout(m_eng_recheck_timeout);
		m_eng_recheck_timeout = -1;
		startEngPolling();
	} else {
		m_eng_last_requested = getCurrentTimestamp();
	}
}

void Asr1802Modem::startEngPolling() {
	if (m_eng_recheck_timeout != -1)
		return;
	
	requestEngInfo();
	
	int timeout = getCurrentTimestamp() - m_eng_last_requested < ENG_INFO_UPDATE_TIMEOUT ? 2000 : 60000;
	m_eng_recheck_timeout = Loop::setTimeout([this]() {
		m_eng_recheck_timeout = -1;
		startEngPolling();
	}, timeout);
}

void Asr1802Modem::requestEngInfo() {
	m_at.sendCommand("AT+EEMGINFO?", "+EEMGINFO");
}

void Asr1802Modem::handleServingCell(const std::string &event) {
	AtParser parser(event);
	
	// LTE
	if (strStartsWith(event, "+EEMLTESVC")) {
		int rsrp;
		int rsrq;
		int main_rsrp;
		int div_rsrp;
		int main_rsrq;
		int div_rsrq;
		
		parser
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseSkip()
			.parseInt(&rsrp)
			.parseInt(&rsrq)
			.parseSkip()
			.parseInt(&main_rsrp)
			.parseInt(&div_rsrp)
			.parseInt(&main_rsrq)
			.parseInt(&div_rsrq);
		
		if (!parser.success())
			return;
		
		m_signal.rsrp_dbm = decodeRSRP(rsrp);
		m_signal.rsrq_db = decodeRSRQ(rsrq);
		
		m_signal.main_rsrp_dbm = decodeRSRP(main_rsrp);
		m_signal.main_rsrq_db = decodeRSRQ(main_rsrq);
		
		m_signal.div_rsrp_dbm = decodeRSRP(div_rsrp);
		m_signal.div_rsrq_db = decodeRSRQ(div_rsrq);
	}
	
	// UMTS
	if (strStartsWith(event, "+EEMUMTSSVC")) {
		bool s_cmeas_present;
		
		parser
			.parseSkip()
			.parseBool(&s_cmeas_present)
			.parseSkip()
			.parseSkip();
		
		if (!parser.success())
			return;
		
		if (s_cmeas_present) {
			int rscp, ecno, rssi;
			
			if (m_is_td_modem) {
				parser.parseInt(&rscp).parseInt(&rssi);
			} else {
				parser.parseInt(&rscp).parseInt(&rssi).parseInt(&ecno);
			}
			
			if (!parser.success())
				return;
			
			m_signal.rssi_dbm = decodeRSSI(rssi);
			m_signal.rscp_dbm = rscp;
			
			if (!m_is_td_modem)
				m_signal.ecio_db = ecno;
		}
	}
}
