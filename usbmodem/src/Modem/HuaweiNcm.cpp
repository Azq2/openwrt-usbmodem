#include "HuaweiNcm.h"
#include "BaseAt.h"

#include <Core/Loop.h>
#include <Core/GsmUtils.h>

bool HuaweiNcmModem::init() {
	const char *init_commands[] = {
		"ATZ",
		"ATS0=0",
		
		// Enable extended error codes
		"AT+CMEE=1",
		
		// Enable all network registration unsolicited events
		"AT+CREG=2",
		"AT+CGREG=2",
		"AT+CEREG=2",
		
		// PDU mode
		"AT+CMGF=0",
		
		// Enable indication
		"AT^CURC=1",
		
		// Setup indication mode of new message to TE
		// "AT+CNMI=1,2,2,1,1",
		"AT+CNMI=0,1,0,2,0",
		
		// USSD mode
		"AT+CUSD=1",
		"AT^USSDMODE=0",
		
		nullptr
	};
	
	// Default AT timeout for this modem
	m_at.setDefaultTimeout(10 * 1000);
	
	if (!execAtList(init_commands, true)) {
		LOGE("Modem init commands failed...\n");
		return false;
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
	m_at.onUnsolicited("+CGEV", [this](const std::string &event) {
		handleCgev(event);
	});
	m_at.onUnsolicited("^HCSQ", [this](const std::string &event) {
		handleHcsq(event);
	});
	m_at.onUnsolicited("+CPIN", [this](const std::string &event) {
		handleCpin(event);
	});
	m_at.onUnsolicited("+CMT", [this](const std::string &event) {
		handleCmt(event);
	});
	m_at.onUnsolicited("+CMTI", [this](const std::string &event) {
		handleCmt(event);
	});
	m_at.onUnsolicited("^NDISSTAT", [this](const std::string &event) {
		Loop::setTimeout([this]() {
			handleConnect();
		}, 0);
	});
	
	Loop::setTimeout([this]() {
		m_at.sendCommandNoResponse("AT+CREG?");
		m_at.sendCommandNoResponse("AT+CGREG?");
		m_at.sendCommandNoResponse("AT+CEREG?");
		m_at.sendCommandNoResponse("AT^HCSQ?");
	}, 0);
	
	on<EvDataDisconnected>([this](const auto &event) {
		startDataConnection();
	});
	
	on<EvTechChanged>([this](const auto &event) {
		startDataConnection();
	});
	
	on<EvSimStateChanged>([this](const auto &event) {
		if (event.state == SIM_READY) {
			Loop::setTimeout([this]() {
				intiSms();
			}, 1000);
		}
	});
	
	if (!syncApn())
		return false;
	
	if (!setRadioOn(true))
		return false;
	
	startSimPolling();
	
	return true;
}

// At this function we have <5s, otherwise netifd send SIGKILL
bool HuaweiNcmModem::close() {
	// Disable unsolicited for prevent side effects
	m_at.resetUnsolicitedHandlers();
	
	// Poweroff radio
	m_at.sendCommandNoResponse("AT+CFUN=0", 5000);
	
	return BaseAtModem::close();
}		

bool HuaweiNcmModem::setRadioOn(bool state) {
	if (isRadioOn())
		return true;
	
	std::string cmd = "AT+CFUN=" + std::to_string(state ? 1 : 0);
	return m_at.sendCommandNoResponse(cmd) == 0;
}

bool HuaweiNcmModem::isRadioOn() {
	auto response = m_at.sendCommand("AT+CFUN?", "+CFUN");
	if (response.error)
		return false;
	
	int cfun_state;
	if (!AtParser(response.data()).parseNextInt(&cfun_state))
		return false;
	
	return cfun_state == 1;
}

int HuaweiNcmModem::getCommandTimeout(const std::string &cmd) {
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

bool HuaweiNcmModem::setOption(const std::string &name, const std::any &value) {
	if (BaseAtModem::setOption(name, value))
		return true;
	
	if (name == "prefer_dhcp") {
		m_prefer_dhcp = std::any_cast<bool>(value);
		return true;
	}
	
	return false;
}

std::vector<HuaweiNcmModem::Capability> HuaweiNcmModem::getCapabilities() {
	return {
		CAP_SMS,
		CAP_USSD,
		CAP_CALLS,
		CAP_SET_NETWORK_MODE,
		CAP_SET_ROAMING_MODE,
		CAP_NETWORK_SEARCH,
		CAP_NETMON,
	};
}
