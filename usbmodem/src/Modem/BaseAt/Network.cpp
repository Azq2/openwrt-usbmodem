#include "../BaseAt.h"
#include <Core/Loop.h>

std::tuple<bool, BaseAtModem::NetworkInfo> BaseAtModem::getNetworkInfo() {
	auto [success, oper] = getCurrentOperator();
	
	return {true, {
		.ipv4	= m_ipv4,
		.ipv6	= m_ipv6,
		.reg	= m_net_reg,
		.tech	= m_tech,
		.signal	= m_signal,
		.oper	= oper
	}};
}

bool BaseAtModem::Creg::isRegistered() const {
	switch (status) {
		case CREG_REGISTERED_HOME:				return true;
		case CREG_REGISTERED_ROAMING:			return true;
		case CREG_REGISTERED_HOME_NO_CSFB:		return true;
		case CREG_REGISTERED_ROAMING_NO_CSFB:	return true;
	}
	return false;
}

BaseAtModem::NetworkTech BaseAtModem::Creg::toNetworkTech() const {
	if (!isRegistered())
		return TECH_NO_SERVICE;
	return cregToTech(tech);
}

BaseAtModem::NetworkReg BaseAtModem::Creg::toNetworkReg() const {
	switch (status) {
		case CREG_NOT_REGISTERED:					return NET_NOT_REGISTERED;
		case CREG_REGISTERED_HOME:					return NET_REGISTERED_HOME;
		case CREG_SEARCHING:						return NET_SEARCHING;
		case CREG_REGISTRATION_DENIED:				return NET_NOT_REGISTERED;
		case CREG_UNKNOWN:							return NET_NOT_REGISTERED;
		case CREG_REGISTERED_ROAMING:				return NET_REGISTERED_ROAMING;
		case CREG_REGISTERED_HOME_SMS_ONLY:			return NET_REGISTERED_HOME;
		case CREG_REGISTERED_ROAMING_SMS_ONLY:		return NET_REGISTERED_ROAMING;
		case CREG_REGISTERED_EMERGENY_ONLY:			return NET_NOT_REGISTERED;
		case CREG_REGISTERED_HOME_NO_CSFB:			return NET_REGISTERED_HOME;
		case CREG_REGISTERED_ROAMING_NO_CSFB:		return NET_REGISTERED_ROAMING;
		case CREG_REGISTERED_EMERGENY_ONLY2:		return NET_NOT_REGISTERED;
	}
	return NET_NOT_REGISTERED;
}

BaseAtModem::NetworkTech BaseAtModem::cregToTech(CregTech creg_tech) {
	switch (creg_tech) {
		case CREG_TECH_GSM:				return TECH_GSM;
		case CREG_TECH_GSM_COMPACT:		return TECH_GSM;
		case CREG_TECH_UMTS:			return TECH_UMTS;
		case CREG_TECH_EDGE:			return TECH_EDGE;
		case CREG_TECH_HSDPA:			return TECH_HSDPA;
		case CREG_TECH_HSUPA:			return TECH_HSUPA;
		case CREG_TECH_HSPA:			return TECH_HSPA;
		case CREG_TECH_HSPAP:			return TECH_HSPAP;
		case CREG_TECH_LTE:				return TECH_LTE;
	}
	return TECH_UNKNOWN;
}

BaseAtModem::CregTech BaseAtModem::techToCreg(NetworkTech tech) {
	switch (tech) {
		case TECH_GSM:		return CREG_TECH_GSM;
		case TECH_UMTS:		return CREG_TECH_UMTS;
		case TECH_EDGE:		return CREG_TECH_EDGE;
		case TECH_HSDPA:	return CREG_TECH_HSDPA;
		case TECH_HSUPA:	return CREG_TECH_HSUPA;
		case TECH_HSPA:		return CREG_TECH_HSPA;
		case TECH_HSPAP:	return CREG_TECH_HSPAP;
		case TECH_LTE:		return CREG_TECH_LTE;
	}
	return CREG_TECH_UNKNOWN;
}

void BaseAtModem::handleCsq(const std::string &event) {
	int rssi, ecio;
	bool parsed = AtParser(event)
		.parseInt(&rssi)
		.parseInt(&ecio)
		.success();
	
	if (!parsed)
		return;
	
	// RSSI (Received signal strength)
	m_signal.rssi_dbm = -(rssi >= 99 ? NAN : 113 - (rssi * 2));
}

void BaseAtModem::handleCesq(const std::string &event) {
	static const double bit_errors[] = {0.14, 0.28, 0.57, 1.13, 2.26, 4.53, 9.05, 18.10};
	int rssi, ber, rscp, ecio, rsrq, rsrp;
	
	bool parsed = AtParser(event)
		.parseInt(&rssi)
		.parseInt(&ber)
		.parseInt(&rscp)
		.parseInt(&ecio)
		.parseInt(&rsrq)
		.parseInt(&rsrp)
		.success();
	
	if (!parsed)
		return;
	
	// RSSI (Received signal strength)
	m_signal.rssi_dbm = -(rssi >= 99 ? NAN : 111 - rssi);
	
	// Bit Error
	m_signal.bit_err_pct = ber >= 0 && ber < COUNT_OF(bit_errors) ? bit_errors[ber] : NAN;
	
	// RSCP (Received signal code power)
	m_signal.rscp_dbm = -(rscp >= 255 ? NAN : 121 - rscp);
	
	// Ec/lo
	m_signal.ecio_db = -(ecio >= 255 ? NAN : (49.0f - (float) ecio) / 2.0f);
	
	// RSRQ (Reference signal received quality)
	m_signal.rsrq_db = -(rsrq >= 255 ? NAN : (40.0f - (float) rsrq) / 2.0f);
	
	// RSRP (Reference signal received power)
	m_signal.rsrp_dbm = -(rsrp >= 255 ? NAN : 141 - rsrp);
	
	emit<EvNetworkSignalChanged>({.signal = m_signal});
}

void BaseAtModem::handleCreg(const std::string &event) {
	Creg *reg = nullptr;
	
	if (strStartsWith(event, "+CREG")) {
		reg = &m_creg;
	} else if (strStartsWith(event, "+CGREG")) {
		reg = &m_cgreg;
	} else if (strStartsWith(event, "+CEREG")) {
		reg = &m_cereg;
	} else {
		// Invalid data
		return;
	}
	
	uint32_t cell_id = 0, loc_id = 0;
	int tech = CREG_TECH_UNKNOWN, stat = CREG_NOT_REGISTERED;
	bool parsed = false;
	
	switch (AtParser::getArgCnt(event)) {
		/* +CREG: <stat> */
		/* +CGREG: <stat> */
		/* +CEREG: <stat> */
		case 1:
			parsed = AtParser(event)
				.parseInt(&stat)
				.success();
		break;
		
		/* +CREG: <n>, <stat> */
		/* +CGREG: <n>, <stat> */
		/* +CEREG: <n>, <stat> */
		case 2:
			parsed = AtParser(event)
				.parseSkip()
				.parseInt(&stat)
				.success();
		break;
		
		/* +CREG: <state>, <lac>, <cid>, <act> */
		/* +CEREG: <state>, <lac>, <cid>, <act> */
		case 4:
			parsed = AtParser(event)
				.parseInt(&stat)
				.parseUInt(&loc_id, 16)
				.parseUInt(&cell_id, 16)
				.parseInt(&tech)
				.success();
		break;
		
		/* +CREG: <n>, <state>, <lac>, <cid>, <act> */
		/* +CEREG: <n>, <state>, <lac>, <cid>, <act> */
		/* +CGREG: <state>, <lac>, <cid>, <act>, <rac> */
		case 5:
			if (strStartsWith(event, "+CGREG")) {
				parsed = AtParser(event)
					.parseInt(&stat)
					.parseUInt(&loc_id, 16)
					.parseUInt(&cell_id, 16)
					.parseInt(&tech)
					.parseSkip()
					.success();
			} else {
				parsed = AtParser(event)
					.parseSkip()
					.parseInt(&stat)
					.parseUInt(&loc_id, 16)
					.parseUInt(&cell_id, 16)
					.parseInt(&tech)
					.success();
			}
		break;
		
		/* +CGREG: <n>, <state>, <lac>, <cid>, <act>, <rac> */
		case 6:
			parsed = AtParser(event)
				.parseSkip()
				.parseInt(&stat)
				.parseUInt(&loc_id, 16)
				.parseUInt(&cell_id, 16)
				.parseInt(&tech)
				.parseSkip()
				.success();
		break;
	}
	
	if (!parsed) {
		LOGE("Invalid CREG: %s\n", event.c_str());
		return;
	}
	
	reg->status = static_cast<CregStatus>(stat);
	reg->tech = static_cast<CregTech>(tech);
	reg->loc_id = loc_id & 0xFFFF;
	reg->cell_id = cell_id & 0xFFFF;
	
	handleNetworkChange();
}

void BaseAtModem::handleNetworkChange() {
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
	} else if (m_creg.isRegistered()) {
		new_tech = m_creg.toNetworkTech();
		new_net_reg = m_creg.toNetworkReg();
		is_registered = true;
	} else {
		new_tech = TECH_NO_SERVICE;
		new_net_reg = m_creg.toNetworkReg();
	}
	
	if (m_net_reg != new_net_reg) {
		m_net_reg = new_net_reg;
		m_net_cache_version++;
		emit<EvNetworkChanged>({.status = m_net_reg});
	}
	
	if (m_tech != new_tech) {
		m_tech = new_tech;
		m_net_cache_version++;
		emit<EvTechChanged>({.tech = m_tech});
	}
}

std::tuple<bool, BaseAtModem::Operator> BaseAtModem::getCurrentOperator() {
	if (m_net_reg == NET_NOT_REGISTERED || m_net_reg == NET_SEARCHING)
		return {true, {}};
	
	return cached<Operator>(__func__, [this]() {
		bool success;
		std::string id;
		std::string name;
		int format;
		int mode;
		int creg;
		
		Operator op;
		AtChannel::Response response;
		
		// Parse long name
		if (m_at.sendCommandNoResponse("AT+COPS=3,0") != 0)
			return std::any();
		
		response = m_at.sendCommand("AT+COPS?", "+COPS");
		if (response.error)
			return std::any();
		
		success = AtParser(response.data())
			.parseSkip()
			.parseSkip()
			.parseString(&name)
			.success();
		
		if (!success)
			return std::any();
		
		// Parse numeric name
		if (m_at.sendCommandNoResponse("AT+COPS=3,2") != 0)
			return std::any();
		
		response = m_at.sendCommand("AT+COPS?", "+COPS");
		if (response.error)
			return std::any();
		
		success = AtParser(response.data())
			.parseInt(&mode)
			.parseInt(&format)
			.parseString(&id)
			.parseInt(&creg)
			.success();
		
		if (!success)
			return std::any();
		
		if (id.size() != 5) {
			LOGE("Invalid MCC/MNC: %s\n", id.c_str());
			return std::any();
		}
		
		Operator value;
		value.mcc = std::stoi(id.substr(0, 3));
		value.mnc = std::stoi(id.substr(3, 2));
		value.name = name;
		value.tech = cregToTech(static_cast<CregTech>(creg));
		value.status = OPERATOR_STATUS_REGISTERED;
		value.reg = (mode == 0 ? OPERATOR_REG_AUTO : OPERATOR_REG_MANUAL);
		return std::any(value);
	}, m_net_cache_version);
}

void BaseAtModem::stopNetWatchdog() {
	if (m_connect_timeout_id >= 0) {
		Loop::clearTimeout(m_connect_timeout_id);
		m_connect_timeout_id = -1;
	}
}

void BaseAtModem::startNetWatchdog() {
	if (m_connect_timeout_id >= 0 || m_connect_timeout <= 0)
		return;
	
	m_connect_timeout_id = Loop::setTimeout([this]() {
		m_connect_timeout_id = -1;
		emit<EvDataConnectTimeout>({});
	}, m_connect_timeout);
}

std::vector<BaseAtModem::Operator> BaseAtModem::searchOperators() {
	return {};
}

bool BaseAtModem::setOperator(int mcc, int mnc, NetworkTech tech) {
	return false;
}

std::vector<BaseAtModem::NetworkModeItem> BaseAtModem::getNetworkModes() {
	return {};
}

bool BaseAtModem::setNetworkMode(int mode_id) {
	return false;
}

bool BaseAtModem::isRoamingEnabled() {
	return false;
}

bool BaseAtModem::setDataRoaming(bool enable) {
	return false;
}