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
	m_at.sendCommandNoResponse("AT+EEMGINFO?");
}

void Asr1802Modem::handleEngInfoStart(const std::string &event) {
	m_neighboring_cell.clear();
}

void Asr1802Modem::handleNeighboringCell(const std::string &event) {
	AtParser parser(event);
	
	// UMTS
	if (strStartsWith(event, "+EEMUMTSINTER") || strStartsWith(event, "+EEMUMTSINTRA")) {
		int rscp, rssi, mcc, mnc, lac, ci, arfcn;
		
		parser
			.parseSkip() // index
			.parseInt(&rscp) // rscp
			.parseInt(&rssi) // rssi
			.parseSkip() // s_rx_lev
			.parseInt(&mcc) // mcc
			.parseInt(&mnc) // mnc
			.parseInt(&lac) // lac
			.parseInt(&ci) // ci
			.parseInt(&arfcn); // arfcn
			
		if (!parser.success())
			return;
		
		if (rscp == -32768 && (rssi == -1 || rssi == 0))
			return;
		
		m_neighboring_cell.resize(m_neighboring_cell.size() + 1);
		
		auto &cell = m_neighboring_cell.back();
		cell.rssi_dbm = rssi == -1 ? NAN : decodeRSSI(rssi);
		cell.rscp_dbm = rscp == -32768 ? NAN : rscp;
		cell.mcc = mcc;
		cell.mnc = mnc;
		cell.loc_id = lac;
		cell.cell_id = ci;
		cell.freq = arfcn;
	}
}

void Asr1802Modem::handleServingCell(const std::string &event) {
	AtParser parser(event);
	
	m_signal.rssi_dbm = NAN;
	m_signal.rscp_dbm = NAN;
	m_signal.ecio_db = NAN;
	
	m_signal.rsrp_dbm = NAN;
	m_signal.rsrq_db = NAN;
	
	m_signal.main_rsrp_dbm = NAN;
	m_signal.main_rsrq_db = NAN;
	
	m_signal.div_rsrp_dbm = NAN;
	m_signal.div_rsrq_db = NAN;
	
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
		bool s_cmeas_present, s_cparam_present;
		
		parser
			.parseSkip()
			.parseBool(&s_cmeas_present)
			.parseBool(&s_cparam_present)
			.parseSkip();
		
		if (!parser.success())
			return;
		
		if (s_cmeas_present) {
			int rscp, ecno, rssi;
			
			parser.parseInt(&rscp).parseInt(&rssi);
			
			if (!m_is_td_modem)
				parser.parseInt(&ecno).parseSkip();
			
			parser.parseSkip().parseSkip();
			
			if (!parser.success())
				return;
			
			m_signal.rssi_dbm = decodeRSSI(rssi);
			m_signal.rscp_dbm = rscp;
			
			if (!m_is_td_modem)
				m_signal.ecio_db = ecno;
			
			m_neighboring_cell.resize(m_neighboring_cell.size() + 1);
		}
		
		if (s_cparam_present) {
			int mcc, mnc, lac, ci, arfcn;
			
			parser
				.parseSkip()
				.parseSkip()
				.parseInt(&mcc)
				.parseInt(&mnc)
				.parseInt(&lac)
				.parseInt(&ci)
				.parseSkip()
				.parseSkip()
				.parseInt(&arfcn);
			
			if (!parser.success())
				return;
			
			auto &cell = m_neighboring_cell.back();
			cell.rssi_dbm = m_signal.rssi_dbm;
			cell.rscp_dbm = m_signal.rscp_dbm;
			cell.mcc = mcc;
			cell.mnc = mnc;
			cell.loc_id = lac;
			cell.cell_id = ci;
			cell.freq = arfcn;
		}
	}
}
