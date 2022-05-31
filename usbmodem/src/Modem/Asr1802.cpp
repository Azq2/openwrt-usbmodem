#include "Asr1802.h"
#include "BaseAt.h"

#include <Core/Loop.h>
#include <Core/GsmUtils.h>

void Asr1802Modem::handleMmsg(const std::string &event) {
	int status;
	bool success = AtParser(event)
		.parseInt(&status)
		.success();
	
	// SMS ready
	if (success && status == 0) {
		Loop::setTimeout([this]() {
			intiSms();
		}, 100);
	}
}

bool Asr1802Modem::init() {
	const char *init_commands[] = {
		// Enable extended error codes
		"AT+CMEE=1",
		
		// Enable all network registration unsolicited events
		"AT+CREG=2",
		"AT+CGREG=2",
		"AT+CEREG=2",
		
		// Enable CGEV events
		"AT+CGEREP=2,0",
		
		// PDU mode
		"AT+CMGF=0",
		
		// Setup indication mode of new message to TE
		// "AT+CNMI=1,2,2,1,1",
		"AT+CNMI=0,1,0,2,0",
		
		// Enable network indicators unsolicited events
		"AT+CIND=1",
		
		// USSD mode
		"AT+CUSD=1",
		
		// Enable background search
		"AT+BGLTEPLMN=1,30",
		
		// Enable "Engineering Mode"
		"AT+EEMOPT=1",
		
		nullptr
	};
	
	// Default AT timeout for this modem
	m_at.setDefaultTimeout(10 * 1000);
	
	// Currently is a fastest way for get internet after "cold" boot when using DCHP
	// Without this DHCP not respond up to 20 sec
	if (getIfaceProto() == IFACE_DHCP)
		m_force_restart_network = true;
	
	if (m_force_restart_network) {
		// Poweroff radio for reset any PDP contexts
		if (!setRadioOn(false))
			return false;
	}
	
	if (!execAtList(init_commands, true)) {
		LOGE("Modem init commands failed...\n");
		return false;
	}
	
	if (!detectModemType())
		return false;
	
	if (!syncApn())
		return false;
	
	auto network_creg_handler = [this](const std::string &event) {
		handleCreg(event);
	};
	m_at.onUnsolicited("+CREG", network_creg_handler);
	m_at.onUnsolicited("+CGREG", network_creg_handler);
	m_at.onUnsolicited("+CEREG", network_creg_handler);
	
	m_at.onUnsolicited("+CUSD", [this](const std::string &event) {
		handleCusd(event);
	});
	m_at.onUnsolicited("+CGEV", [this](const std::string &event) {
		handleCgev(event);
	});
	m_at.onUnsolicited("+CESQ", [this](const std::string &event) {
		handleCesq(event);
	});
	m_at.onUnsolicited("+CPIN", [this](const std::string &event) {
		handleCpin(event);
	});
	m_at.onUnsolicited("+MMSG", [this](const std::string &event) {
		handleMmsg(event);
	});
	m_at.onUnsolicited("+CMT", [this](const std::string &event) {
		handleCmt(event);
	});
	m_at.onUnsolicited("+CMTI", [this](const std::string &event) {
		handleCmt(event);
	});
	
	m_at.onUnsolicited("+EEMGINFO", [this](const std::string &event) {
		handleEngInfoStart(event);
	});
	
	m_at.onUnsolicited("+EEMGINFO ", [this](const std::string &event) {
		handleEngInfoStart(event);
	});
	
	// Serving Cell
	m_at.onUnsolicited("+EEMLTESVC", [this](const std::string &event) {
		handleServingCell(event);
	});
	m_at.onUnsolicited("+EEMUMTSSVC", [this](const std::string &event) {
		handleServingCell(event);
	});
	m_at.onUnsolicited("+EEMGINFOSVC", [this](const std::string &event) {
		handleServingCell(event);
	});
	
	// Neighboring Cell
	m_at.onUnsolicited("+EEMUMTSINTER", [this](const std::string &event) {
		handleNeighboringCell(event);
	});
	m_at.onUnsolicited("+EEMUMTSINTRA", [this](const std::string &event) {
		handleNeighboringCell(event);
	});
	
	if (!m_force_restart_network) {
		Loop::setTimeout([this]() {
			// Detect, if already have internet
			if (m_data_state == DISCONNECTED) {
				int cid = getCurrentPdpCid();
				if (cid > 0) {
					handleConnect();
				} else if (cid < 0) {
					restartNetwork();
				}
			}
			
			// Sync state
			m_at.sendCommandNoResponse("AT+CREG?");
			m_at.sendCommandNoResponse("AT+CGREG?");
			m_at.sendCommandNoResponse("AT+CEREG?");
			m_at.sendCommandNoResponse("AT+CESQ");
		}, 0);
	}
	
	on<EvDataDisconnected>([this](const auto &event) {
		startDataConnection();
		startNetWatchdog();
	});
	
	on<EvDataConnected>([this](const auto &event) {
		stopNetWatchdog();
	});
	
	on<EvTechChanged>([this](const auto &event) {
		startDataConnection();
	});
	
	on<EvSimStateChaned>([this](const auto &event) {
		if (event.state == SIM_READY) {
			Loop::setTimeout([this]() {
				intiSms();
			}, 1000);
		}
	});
	
	// Poweron
	if (!setRadioOn(true))
		return false;
	
	startNetWatchdog();
	startSimPolling();
	startEngPolling();
	
	return true;
}

// At this function we have <5s, otherwise netifd send SIGKILL
bool Asr1802Modem::close() {
	// Disable unsolicited for prevent side effects
	m_at.resetUnsolicitedHandlers();
	
	// Poweroff radio
	m_at.sendCommandNoResponse("AT+CFUN=4", 5000);
	
	return BaseAtModem::close();
}		

bool Asr1802Modem::setRadioOn(bool state) {
	if (isRadioOn())
		return true;
	
	std::string cmd = "AT+CFUN=" + std::to_string(state ? 1 : 4);
	return m_at.sendCommandNoResponse(cmd) == 0;
}

bool Asr1802Modem::isRadioOn() {
	auto response = m_at.sendCommand("AT+CFUN?", "+CFUN");
	if (response.error)
		return false;
	
	int cfun_state;
	if (!AtParser(response.data()).parseNextInt(&cfun_state))
		return false;
	
	return cfun_state == 1;
}

void Asr1802Modem::handleUssdResponse(int code, const std::string &data, int dcs) {
	// Fix broken USSD dcs
	if (dcs == 17)
		dcs = 72; /* UCS2 */
	
	if (dcs == 0)
		dcs = 68; /* 8bit GSM */
	
	BaseAtModem::handleUssdResponse(code, data, dcs);
}

int Asr1802Modem::getCommandTimeout(const std::string &cmd) {
	if (strStartsWith(cmd, "AT+CFUN"))
		return 50 * 1000;
	
	if (strStartsWith(cmd, "AT+CGACT"))
		return 110 * 1000;
	
	if (strStartsWith(cmd, "AT+CGATT"))
		return 110 * 1000;
	
	if (strStartsWith(cmd, "AT+CGDATA"))
		return 185 * 1000;
	
	if (strStartsWith(cmd, "AT+CUSD"))
		return 110 * 1000;
	
	return BaseAtModem::getCommandTimeout(cmd);
}

bool Asr1802Modem::setOption(const std::string &name, const std::any &value) {
	if (BaseAtModem::setOption(name, value))
		return true;
	
	if (name == "force_restart_network") {
		m_force_restart_network = std::any_cast<bool>(value);
		return true;
	} else if (name == "prefer_dhcp") {
		m_prefer_dhcp = std::any_cast<bool>(value);
		return true;
	}
	
	return false;
}
