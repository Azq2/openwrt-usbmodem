#include "../BaseAt.h"
#include <Core/Loop.h>

std::tuple<bool, BaseAtModem::NetworkInfo> BaseAtModem::getNetworkInfo() {
	auto [success, oper] = getCurrentOperator();
	
	return {true, {
		.ipv4		= m_ipv4,
		.ipv6		= m_ipv6,
		.reg		= m_net_reg,
		.tech		= m_tech,
		.signal		= m_signal,
		.oper		= oper,
		.cell		= m_cell_info
	}};
}

bool BaseAtModem::Creg::isRegistered() const {
	switch (status) {
		case CREG_REGISTERED_HOME:				return true;
		case CREG_REGISTERED_ROAMING:			return true;
		case CREG_REGISTERED_HOME_NO_CSFB:		return true;
		case CREG_REGISTERED_ROAMING_NO_CSFB:	return true;
		case CREG_REGISTERED_EMERGENY_ONLY:		return true;
		case CREG_REGISTERED_EMERGENY_ONLY2:	return true;
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
		case CREG_REGISTERED_EMERGENY_ONLY:			return NET_EMERGENY_ONLY;
		case CREG_REGISTERED_HOME_NO_CSFB:			return NET_REGISTERED_HOME;
		case CREG_REGISTERED_ROAMING_NO_CSFB:		return NET_REGISTERED_ROAMING;
		case CREG_REGISTERED_EMERGENY_ONLY2:		return NET_EMERGENY_ONLY;
	}
	return NET_NOT_REGISTERED;
}

BaseAtModem::NetworkTech BaseAtModem::cregToTech(CregTech creg_tech) {
	switch (creg_tech) {
		case CREG_TECH_GSM:				return TECH_GSM;
		case CREG_TECH_GSM_COMPACT:		return TECH_GSM_COMPACT;
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
		case TECH_GSM:			return CREG_TECH_GSM;
		case TECH_GSM_COMPACT:	return CREG_TECH_GSM_COMPACT;
		case TECH_UMTS:			return CREG_TECH_UMTS;
		case TECH_EDGE:			return CREG_TECH_EDGE;
		case TECH_HSDPA:		return CREG_TECH_HSDPA;
		case TECH_HSUPA:		return CREG_TECH_HSUPA;
		case TECH_HSPA:			return CREG_TECH_HSPA;
		case TECH_HSPAP:		return CREG_TECH_HSPAP;
		case TECH_LTE:			return CREG_TECH_LTE;
	}
	return CREG_TECH_UNKNOWN;
}

void BaseAtModem::handleCsq(const std::string &event) {
	int rssi, ber;
	bool parsed = AtParser(event)
		.parseInt(&rssi)
		.parseInt(&ber)
		.success();
	
	if (!parsed)
		return;
	
	// RSSI (Received signal strength)
	m_signal.rssi_dbm = decodeSignal(rssi, 113, 2, 99);
	
	// Bit Error
	m_signal.bit_err_pct = decodeRERR(ber);
}

void BaseAtModem::handleCreg(const std::string &event) {
	Creg *reg = nullptr;
	bool is_manual = false;
	
	if (strStartsWith(event, "+CREG")) {
		reg = &m_creg;
		is_manual = (m_manual_creg_req == "CREG");
		m_manual_creg_req = "";
	} else if (strStartsWith(event, "+CGREG")) {
		reg = &m_cgreg;
		is_manual = (m_manual_creg_req == "CGREG");
		m_manual_creg_req = "";
	} else if (strStartsWith(event, "+CEREG")) {
		reg = &m_cereg;
		is_manual = (m_manual_creg_req == "CEREG");
		m_manual_creg_req = "";
	} else {
		// Invalid data
		return;
	}
	
	uint32_t cell_id = 0, loc_id = 0;
	int tech = CREG_TECH_UNKNOWN, stat = CREG_NOT_REGISTERED;
	bool parsed = false;
	
	AtParser parser(event);
	
	int arg_count = AtParser::getArgCnt(event);
	if (is_manual) {
		// skip <n>
		if (!parser.parseNextSkip())
			return;
		arg_count--;
	}
	
	switch (arg_count) {
		// <stat>
		case 1:
			parsed = parser
				.parseInt(&stat)
				.success();
		break;
		
		// <stat>,<lac>,<ci>
		case 3:
			parsed = parser
				.parseInt(&stat)
				.parseUInt(&loc_id, 16)
				.parseUInt(&cell_id, 16)
				.success();
		break;
		
		// <stat>,<lac>,<ci>,<act>
		// <stat>,<lac>,<ci>,<act>,<rac>
		case 4:
		case 5:
			parsed = parser
				.parseInt(&stat)
				.parseUInt(&loc_id, 16)
				.parseUInt(&cell_id, 16)
				.parseInt(&tech)
				.success();
		break;
		
		default:
			LOGE("Invalid CREG format: %s\n", event.c_str());
		break;
	}
	
	if (!parsed) {
		LOGE("Invalid CREG: %s\n", event.c_str());
		return;
	}
	
	reg->status = static_cast<CregStatus>(stat);
	reg->tech = static_cast<CregTech>(tech);
	reg->loc_id = loc_id;
	reg->cell_id = cell_id;
	
	Loop::setTimeout([this]() {
		handleNetworkChange();
	}, 0);
}

void BaseAtModem::handleNetworkChange() {
	NetworkTech new_tech;
	NetworkReg new_net_reg;
	
	if (m_cereg.isRegistered()) {
		new_tech = m_cereg.toNetworkTech();
		new_net_reg = m_cereg.toNetworkReg();
		m_cell_info.cell_id = m_cereg.cell_id;
		m_cell_info.loc_id = m_cereg.loc_id;
	} else if (m_cgreg.isRegistered()) {
		new_tech = m_cgreg.toNetworkTech();
		new_net_reg = m_cgreg.toNetworkReg();
		m_cell_info.cell_id = m_cgreg.cell_id;
		m_cell_info.loc_id = m_cgreg.loc_id;
	} else {
		new_tech = m_creg.toNetworkTech();
		new_net_reg = m_creg.toNetworkReg();
		m_cell_info.cell_id = m_creg.cell_id;
		m_cell_info.loc_id = m_creg.loc_id;
	}
	
	// Some modems not reporting <act> on CREG :(
	if (new_tech == TECH_UNKNOWN)
		new_tech = getTechFromCops();
	
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

BaseAtModem::NetworkTech BaseAtModem::getTechFromCops() {
	auto response = m_at.sendCommand("AT+COPS?", "+COPS");
	if (response.error)
		return TECH_UNKNOWN;
	
	if (AtParser::getArgCnt(response.data()) <= 3)
		return TECH_GSM;
	
	int tech;
	bool success = AtParser(response.data())
			.parseSkip() // mode
			.parseSkip() // format
			.parseSkip() // name
			.parseInt(&tech) // tech
			.success();
	return success ? cregToTech(static_cast<CregTech>(tech)) : TECH_UNKNOWN;
}

std::tuple<bool, BaseAtModem::Operator> BaseAtModem::getCurrentOperator() {
	if (m_net_reg == NET_NOT_REGISTERED || m_net_reg == NET_SEARCHING)
		return {true, {}};
	
	return cached<Operator>(__func__, [this]() {
		bool success;
		std::string name;
		int format;
		int mode;
		int tech;
		
		auto response = m_at.sendCommandMultiline("AT+COPS=3,0;+COPS?;+COPS=3,2;+COPS?", "+COPS");
		if (response.error)
			return std::any();
		
		Operator value;
		for (auto &line: response.lines) {
			if (AtParser::getArgCnt(line) < 3)
				return std::any();
			
			if (AtParser::getArgCnt(line) == 3) {
				success = AtParser(line)
					.parseInt(&mode)
					.parseInt(&format)
					.parseString(&name)
					.success();
			} else {
				success = AtParser(line)
					.parseInt(&mode)
					.parseInt(&format)
					.parseString(&name)
					.parseInt(&tech)
					.success();
			}
			
			if (!success)
				return std::any();
			
			if (format == 2) {
				value.mcc = strToInt(name.substr(0, 3), 10, -1);
				value.mnc = strToInt(name.substr(3), 10, -1);
			} else {
				value.name = name;
			}
			
			value.tech = cregToTech(static_cast<CregTech>(tech));
			value.status = OPERATOR_STATUS_REGISTERED;
			value.reg = (mode == 0 ? OPERATOR_REG_AUTO : OPERATOR_REG_MANUAL);
		}
		
		if (!value.name.size())
			value.name = strprintf("%03d %02d", value.mcc, value.mnc);
		
		return std::any(value);
	}, m_net_cache_version);
}

std::tuple<bool, std::vector<BaseAtModem::Operator>> BaseAtModem::searchOperators() {
	std::vector<Operator> operators;
	std::vector<std::string> operators_raw;
	
	auto response = m_at.sendCommand("AT+COPS=?", "+COPS");
	if (response.error)
		return {false, {}};
	
	if (!AtParser(response.data()).parseNextList(&operators_raw))
		return {false, {}};
	
	for (auto opearator_info: operators_raw) {
		int status = OPERATOR_STATUS_UNKNOWN;
		int tech = CREG_TECH_UNKNOWN;
		std::string name_short;
		std::string name_long;
		std::string name_numeric;
		
		if (!opearator_info.size())
			break;
		
		bool success;
		if (AtParser::getArgCnt(opearator_info) == 4) {
			success = AtParser(opearator_info)
				.reset()
				.parseInt(&status, 10)
				.parseString(&name_long)
				.parseString(&name_short)
				.parseString(&name_numeric)
				.success();
		} else {
			success = AtParser(opearator_info)
				.reset()
				.parseInt(&status, 10)
				.parseString(&name_long)
				.parseString(&name_short)
				.parseString(&name_numeric)
				.parseInt(&tech, 10)
				.success();
		}
		
		if (name_numeric.size() != 5)
			success = false;
		
		if (success) {
			operators.resize(operators.size() + 1);
			
			Operator &op = operators.back();
			op.mcc = strToInt(name_numeric.substr(0, 3), 10, -1);
			op.mnc = strToInt(name_numeric.substr(3), 10, -1);
			op.name = name_long;
			op.tech = cregToTech(static_cast<CregTech>(tech));
			op.status = OPERATOR_STATUS_UNKNOWN;
			op.reg = OPERATOR_REG_NONE;
			
			if ((status >= 0 && status < 4))
				op.status = static_cast<OperatorStatus>(status);
		} else {
			LOGE("Invalid operator entry: %s\n", opearator_info.c_str());
		}
	}
	
	return {true, operators};
}

bool BaseAtModem::setOperator(OperatorRegMode mode, int mcc, int mnc, NetworkTech tech) {
	CregTech act = techToCreg(tech);
	
	if (mode == OPERATOR_REG_NONE) {
		return m_at.sendCommandNoResponse("AT+COPS=2") == 0;
	} else if (mode == OPERATOR_REG_AUTO) {
		if (m_at.sendCommandNoResponse("AT+COPS=2") != 0)
			return false;
		return m_at.sendCommandNoResponse("AT+COPS=0") == 0;
	} else {
		std::string at_cmd;
		if (act != CREG_TECH_UNKNOWN) {
			at_cmd = strprintf("AT+COPS=1,2,\"%03d%02d\",%d", mcc, mnc, static_cast<int>(act));
		} else {
			at_cmd = strprintf("AT+COPS=1,2,\"%03d%02d\"", mcc, mnc);
		}
		return m_at.sendCommandNoResponse(at_cmd) == 0;
	}
}

std::tuple<bool, std::vector<BaseAtModem::NetworkMode>> BaseAtModem::getNetworkModes() {
	return {true, {NET_MODE_AUTO}};
}

std::tuple<bool, BaseAtModem::NetworkMode> BaseAtModem::getCurrentNetworkMode() {
	return {true, NET_MODE_AUTO};
}

bool BaseAtModem::setNetworkMode(NetworkMode new_mode) {
	return true;
}

std::tuple<bool, bool> BaseAtModem::isRoamingEnabled() {
	return {true, false};
}

bool BaseAtModem::setDataRoaming(bool enable) {
	return true;
}

std::tuple<bool, std::vector<BaseAtModem::NetworkNeighborCell>> BaseAtModem::getNeighboringCell() {
	return {true, {}};
}
