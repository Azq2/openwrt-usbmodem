#include "../BaseAt.h"
#include <Core/Loop.h>

std::tuple<bool, BaseAtModem::SimInfo> BaseAtModem::getSimInfo() {
	if (m_sim_state == SIM_READY) {
		return cached<SimInfo>(__func__, [this]() {
			SimInfo info;
			AtChannel::Response response;
			
			response = m_at.sendCommand("AT+CNUM", "+CNUM");
			if (response.error || !AtParser(response.data()).parseSkip().parseString(&info.number).success())
				info.number = "";
			
			response = m_at.sendCommandNumericOrWithPrefix("AT+CIMI", "+CIMI");
			if (response.error || !AtParser(response.data()).parseString(&info.imsi).success())
				info.imsi = "";
			
			info.state = m_sim_state;
			
			return info;
		});
	}
	return {true, {.state = m_sim_state}};
}

void BaseAtModem::startSimPolling() {
	if (m_sim_state != SIM_NOT_INITIALIZED && m_sim_state != SIM_WAIT_UNLOCK)
		return;
	
	auto response = m_at.sendCommand("AT+CPIN?");
	if (response.isCmeError()) {
		int error = response.getCmeError();
		
		switch (error) {
			case 10:	// SIM not inserted
				LOGD("SIM not present\n");
				setSimState(SIM_REMOVED);
			break;
			
			case 13:	// SIM failure
			case 15:	// SIM wrong
				LOGD("SIM not failure\n");
				setSimState(SIM_ERROR);
			break;
			
			case 14:	// SUM busy
				// SIM not ready, ignore this error
			break;
			
			default:
				LOGE("AT+CPIN command failed [CME ERROR %d]\n", error);
				setSimState(SIM_ERROR);
			break;
		}
	} else if (response.error) {
		// AT+CPIN not supported
		setSimState(SIM_ERROR);
	}
	
	Loop::setTimeout([this]() {
		startSimPolling();
	}, 1000);
}

void BaseAtModem::setSimState(SimState new_state) {
	if (new_state != m_sim_state) {
		m_sim_state = new_state;
		emit<EvSimStateChaned>({.state = m_sim_state});
	}
}

bool BaseAtModem::handleSimLock(const std::string &code) {
	if (strStartsWith(code, "SIM PIN")) {
		setSimState(SIM_PIN1_LOCK);
		
		if (!m_pincode.size()) {
			LOGE("SIM pin required, but no pincode specified...\n");
			return true;
		}
		
		if (m_pincode_entered)
			return true;
		
		m_pincode_entered = true;
		
		Loop::setTimeout([this]() {
			if (m_at.sendCommandNoResponse("AT+CPIN=" + m_pincode) != 0)
				LOGE("SIM PIN unlock error\n");
			
			setSimState(SIM_WAIT_UNLOCK);
			startSimPolling();
		}, 0);
		
		return true;
	} else if (strStartsWith(code, "SIM REMOVED")) {
		setSimState(SIM_REMOVED);
		return true;
	} else if (strStartsWith(code, "SIM PIN2")) {
		setSimState(SIM_PIN2_LOCK);
		return true;
	} else if (strStartsWith(code, "SIM PUK")) {
		setSimState(SIM_PUK1_LOCK);
		return true;
	} else if (strStartsWith(code, "SIM PUK2")) {
		setSimState(SIM_PUK2_LOCK);
		return true;
	} else if (strStartsWith(code, "PH-NET PIN")) {
		setSimState(SIM_MEP_LOCK);
		return true;
	}
	return false;
}

void BaseAtModem::handleCpin(const std::string &event) {
	std::string code;
	if (!AtParser(event).parseString(&code).success()) {
		LOGE("Invalid +CPIN: %s\n", event.c_str());
		setSimState(SIM_ERROR);
		return;
	}
	
	if (strStartsWith(code, "READY")) {
		setSimState(SIM_READY);
	} else {
		if (!handleSimLock(code)) {
			LOGE("SIM required other lock code: %s\n", code.c_str());
			setSimState(SIM_OTHER_LOCK);
		}
	}
}
