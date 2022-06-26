#include "GenericPpp.h"
#include "BaseAt.h"

#include <Core/Loop.h>
#include <Core/GsmUtils.h>

std::map<GenericPppModem::NetworkMode, int> GenericPppModem::m_zte_mode2id = {
	{GenericPppModem::NET_MODE_AUTO, 0},
	{GenericPppModem::NET_MODE_ONLY_2G, 1},
	{GenericPppModem::NET_MODE_ONLY_3G, 2},
};

bool GenericPppModem::init() {
	m_at.setDefaultTimeout(10 * 1000);
	
	const char *init_commands[] = {
		// Enable extended error codes
		"AT+CMEE=1",
		
		// Enable all network registration unsolicited events
		"AT+CREG=2|AT+CREG=1|",
		"AT+CGREG=2|AT+CGREG=1|",
		"AT+CEREG=2|AT+CEREG=1|",
		
		// Setup indication mode of new message to TE
		"AT+CNMI=0,1,0,2,0|",
		
		nullptr
	};
	
	if (!execAtList(init_commands, true)) {
		LOGE("Modem init commands failed...\n");
		return false;
	}
	
	m_vendor = getModemVendor();
	
	support_ussd = m_at.sendCommandNoResponse("AT+CUSD=1") == 0;
	support_sms = m_at.sendCommandNoResponse("AT+CMGF=0") == 0;
	
	switch (m_vendor) {
		case VENDOR_ZTE:
			// Extended RSSI level
			support_zrssi = m_at.sendCommandNoResponse("AT+ZRSSI") == 0;
			
			// Network mode
			support_zsnt = m_at.sendCommandNoResponse("AT+ZSNT?") == 0;
		break;
	}
	
	auto network_creg_handler = [this](const std::string &event) {
		handleCreg(event);
	};
	m_at.onUnsolicited("+CREG", network_creg_handler);
	m_at.onUnsolicited("+CGREG", network_creg_handler);
	m_at.onUnsolicited("+CEREG", network_creg_handler);
	
	m_at.onUnsolicited("+CUSD", [this](const std::string &event) {
		handleCusd(event);
	});
	
	if (support_zrssi) {
		m_at.onUnsolicited("+ZRSSI", [this](const std::string &event) {
			handleZrssi(event);
		});
	} else {
		m_at.onUnsolicited("+CSQ", [this](const std::string &event) {
			handleCsq(event);
		});
	}
	
	m_at.onUnsolicited("+CPIN", [this](const std::string &event) {
		handleCpin(event);
	});
	m_at.onUnsolicited("+CMT", [this](const std::string &event) {
		handleCmt(event);
	});
	m_at.onUnsolicited("+CMTI", [this](const std::string &event) {
		handleCmt(event);
	});
	
	// Sync state
	m_at.sendCommandNoResponse("AT+CREG?");
	m_at.sendCommandNoResponse("AT+CGREG?");
	m_at.sendCommandNoResponse("AT+CEREG?");
	
	requestSignalInfo();
	
	on<EvSimStateChanged>([this](const auto &event) {
		if (event.state == SIM_READY) {
			Loop::setTimeout([this]() {
				intiSms();
			}, 1000);
		}
	});
	
	startSimPolling();
	
	return true;
}

void GenericPppModem::handleZrssi(const std::string &event) {
	int raw_rssi, raw_ecio, raw_rscp;
	
	bool success = AtParser(event)
		.parseInt(&raw_rssi, 10)
		.parseInt(&raw_ecio, 10)
		.parseInt(&raw_rscp, 10)
		.success();
	if (!success)
		return;
	
	// RSSI (Received signal strength)
	m_signal.rssi_dbm = -raw_rssi;
	
	// Ec/lo
	m_signal.ecio_db = (raw_ecio == 1000 ? NAN : -raw_ecio / 2.0f);
	
	// RSCP (Received signal code power)
	m_signal.rscp_dbm = (raw_rscp == 1000 ? NAN : -raw_rscp / 2.0f);
}

void GenericPppModem::wakeSignalTimer() {
	if (getCurrentTimestamp() - m_signal_last_requested >= SIGNAL_INFO_UPDATE_IDLE_TIMEOUT) {
		m_signal_last_requested = getCurrentTimestamp();
		Loop::clearTimeout(m_signal_recheck_timeout);
		m_signal_recheck_timeout = -1;
		startSignalPolling();
	} else {
		m_signal_last_requested = getCurrentTimestamp();
	}
}

void GenericPppModem::startSignalPolling() {
	if (m_signal_recheck_timeout != -1)
		return;
	
	requestSignalInfo();
	
	int timeout = getCurrentTimestamp() - m_signal_last_requested < SIGNAL_INFO_UPDATE_IDLE_TIMEOUT ?
		SIGNAL_INFO_UPDATE_INTERVAL : SIGNAL_INFO_UPDATE_INTERVAL_IDLE;
	m_signal_recheck_timeout = Loop::setTimeout([this]() {
		m_signal_recheck_timeout = -1;
		startSignalPolling();
	}, timeout);
}

void GenericPppModem::requestSignalInfo() {
	if (support_cesq) {
		m_at.sendCommandNoResponse("AT+CESQ");
	} else if (support_zrssi) {
		m_at.sendCommandNoResponse("AT+ZRSSI");
	} else {
		m_at.sendCommandNoResponse("AT+CSQ");
	}
}

std::tuple<bool, GenericPppModem::NetworkInfo> GenericPppModem::getNetworkInfo() {
	wakeSignalTimer();
	return BaseAtModem::getNetworkInfo();
}

GenericPppModem::Vendor GenericPppModem::getModemVendor() {
	auto response = m_at.sendCommandNoPrefix("AT+CGMI");
	if (!response.error) {
		if (strcasestr(response.data().c_str(), "ZTE"))
			return VENDOR_ZTE;
		if (strcasestr(response.data().c_str(), "HUAWEI"))
			return VENDOR_HUAWEI;
	}
	return VENDOR_UNKNOWN;
}

std::tuple<bool, std::vector<GenericPppModem::NetworkMode>> GenericPppModem::getNetworkModes() {
	if (support_zsnt) {
		return {true, {
			NET_MODE_AUTO,
			NET_MODE_ONLY_2G,
			NET_MODE_ONLY_3G,
		}};
	}
	return {true, {}};
}

bool GenericPppModem::setNetworkMode(NetworkMode new_mode) {
	if (support_zsnt) {
		if (m_zte_mode2id.find(new_mode) == m_zte_mode2id.end())
			return false;
		
		std::string query = strprintf("AT+ZSNT=%d,0,0", m_zte_mode2id[new_mode]);
		return m_at.sendCommandNoResponse(query) == 0;
	}
	return true;
}

std::tuple<bool, GenericPppModem::NetworkMode> GenericPppModem::getCurrentNetworkMode() {
	if (support_zsnt) {
		auto response = m_at.sendCommand("AT+ZSNT?", "+ZSNT");
		if (response.error)
			return {false, NET_MODE_UNKNOWN};
		
		int mode;
		if (!AtParser(response.data()).parseNextInt(&mode))
			return {false, NET_MODE_UNKNOWN};
		
		for (auto &it: m_zte_mode2id) {
			if (it.second == mode)
				return {true, it.first};
		}
		
		return {false, NET_MODE_UNKNOWN};
	}
	return {true, NET_MODE_AUTO};
}

// At this function we have <5s, otherwise netifd send SIGKILL
bool GenericPppModem::close() {
	return BaseAtModem::close();
}		

int GenericPppModem::getCommandTimeout(const std::string &cmd) {
	if (strStartsWith(cmd, "AT+CUSD"))
		return 110 * 1000;
	return BaseAtModem::getCommandTimeout(cmd);
}

bool GenericPppModem::setOption(const std::string &name, const std::any &value) {
	if (BaseAtModem::setOption(name, value))
		return true;
	return false;
}

std::vector<GenericPppModem::Capability> GenericPppModem::getCapabilities() {
	std::vector<Capability> caps = {};
	
	if (m_net_type == MODEM_NET_GSM) {
		caps.push_back(CAP_NETWORK_SEARCH);
		
		if (support_zsnt)
			caps.push_back(CAP_SET_NETWORK_MODE);
	}
	
	if (support_sms)
		caps.push_back(CAP_SMS);
	
	if (support_ussd)
		caps.push_back(CAP_USSD);
	
	return caps;
}
