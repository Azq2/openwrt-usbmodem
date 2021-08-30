#include "BaseAt.h"
#include "../Loop.h"
#include "../GsmUtils.h"

#include "zlib.h"

bool ModemBaseAt::Creg::isRegistered() const {
	switch (status) {
		case CREG_REGISTERED_HOME:				return true;
		case CREG_REGISTERED_ROAMING:			return true;
		case CREG_REGISTERED_HOME_NO_CSFB:		return true;
		case CREG_REGISTERED_ROAMING_NO_CSFB:	return true;
	}
	return false;
}

Modem::NetworkTech ModemBaseAt::Creg::toNetworkTech() const {
	if (!isRegistered())
		return TECH_NO_SERVICE;
	
	switch (tech) {
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

Modem::NetworkReg ModemBaseAt::Creg::toNetworkReg() const {
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

ModemBaseAt::ModemBaseAt() {
	m_at.setVerbose(true);
	m_at.setSerial(&m_serial);
	m_at.setDefaultTimeoutCallback([=](const std::string &cmd) {
		return getCommandTimeout(cmd);
	});
}

ModemBaseAt::~ModemBaseAt() {
	
}

/*
 * Get timeout by command
 * */
int ModemBaseAt::getCommandTimeout(const std::string &cmd) {
	// For handshake
	if (cmd == "AT" || strStartsWith(cmd, "ATQ") || strStartsWith(cmd, "ATV") || strStartsWith(cmd, "ATE"))
		return 350;
	
	// Default timeout for unsolicited USSD response
	if (cmd == "+CUSD")
		return 110 * 1000;
	
	// Default timeout
	return 0;
}

/*
 * Custom options
 * */
bool ModemBaseAt::setCustomOption(const std::string &name, const std::any &value) {
	if (name == "connect_timeout") {
		m_connect_timeout = std::any_cast<int>(value);
		return true;
	}
	return false;
}

/*
 * Check AT channel
 * */
bool ModemBaseAt::ping(int tries) {
	for (int i = 0; i < tries; i++) {
		if (m_at.sendCommandNoResponse("AT") == 0)
			return true;
	}
	return false;
}

bool ModemBaseAt::handshake() {
	m_self_test = true;
	
	bool success = true;
	for (int i = 0; i < 3; i++) {
		success = true;
		
		// enable result codes
		if (m_at.sendCommandNoResponse("ATQ0") != 0)
			success = false;
		
		// enable verbose
		if (m_at.sendCommandNoResponse("ATV1") != 0)
			success = false;
		
		// echo off
		if (m_at.sendCommandNoResponse("ATE0") != 0)
			success = false;
		
		if (success)
			break;
	}
	
	m_self_test = false;
	
	return success;
}

/*
 * Network watchdog
 * */
void ModemBaseAt::stopNetRegWhatchdog() {
	if (m_connect_timeout_id >= 0) {
		Loop::clearTimeout(m_connect_timeout_id);
		m_connect_timeout_id = -1;
	}
}

void ModemBaseAt::startNetRegWhatchdog() {
	if (m_connect_timeout_id >= 0 || m_connect_timeout <= 0)
		return;
	
	m_connect_timeout_id = Loop::setTimeout([=]() {
		m_connect_timeout_id = -1;
		emit<EvDataConnectTimeout>({});
	}, m_connect_timeout);
}

/*
 * Raw AT commands
 * */
std::pair<bool, std::string> ModemBaseAt::sendAtCommand(const std::string &cmd, int timeout) {
	auto response = m_at.sendCommandNoPrefix(cmd, timeout);
	
	std::string out;
	
	if (response.lines.size() > 0) {
		out = strJoin(response.lines, "\n") + "\n" + response.status;
	} else {
		out = response.status;
	}
	
	return std::make_pair(response.error == 0, out);
}

/*
 * SMS
 * */
bool ModemBaseAt::decodeSmsToPdu(const std::string &data, SmsDir *dir, Pdu *pdu, int *id, uint32_t *hash) {
	int stat;
	std::string pdu_bytes;
	bool direction;
	
	bool success = AtParser(data)
		.parseInt(id)
		.parseInt(&stat)
		.parseSkip()
		.parseSkip()
		.parseNewLine()
		.parseString(&pdu_bytes)
		.success();
	
	if (!success)
		return false;
	
	switch (stat) {
		case 0:
			*dir = SMS_DIR_UNREAD;
			direction = false;
		break;
		case 1:
			*dir = SMS_DIR_READ;
			direction = false;
		break;
		case 2:
			*dir = SMS_DIR_UNSENT;
			direction = true;
		break;
		case 3:
			*dir = SMS_DIR_SENT;
			direction = true;
		break;
		default:
			LOGE("Unknown SMS <stat>: '%s'\n", data.c_str());
			return false;
		break;
	}
	
	if (!decodePdu(hex2bin(pdu_bytes), pdu, direction)) {
		LOGE("Invalid PDU in SMS: '%s'\n", pdu_bytes.c_str());
		return false;
	}
	
	if (pdu->type != PDU_TYPE_DELIVER && pdu->type != PDU_TYPE_SUBMIT) {
		LOGE("Unsupported PDU type in SMS: '%s'\n", pdu_bytes.c_str());
		return false;
	}
	
	// Calculate PDU hash
	*hash = crc32(0, reinterpret_cast<const uint8_t *>(id), sizeof(*id));
	*hash = crc32(*hash, reinterpret_cast<const uint8_t *>(pdu_bytes.c_str()), pdu_bytes.size());
	
	return true;
}

void ModemBaseAt::getSmsList(SmsDir from_dir, SmsReadCallback callback) {
	if (from_dir > SMS_DIR_ALL) {
		callback(false, {});
		return;
	}
	
	auto response = m_at.sendCommandMultiline("AT+CMGL=" + std::to_string(from_dir), "+CMGL");
	if (response.error) {
		callback(false, {});
		return;
	}
	
	Loop::setTimeout([=]() {
		auto start = getCurrentTimestamp();
		
		// <smsc>, <addr>, <ref_id>, <parts>
		std::vector<Sms> sms_list;
		std::map<std::tuple<uint8_t, std::string, std::string, uint16_t, uint8_t>, size_t> sms_parts;
		std::tuple<uint8_t, std::string, std::string, uint16_t, uint8_t> sms_key;
		
		sms_list.reserve(response.lines.size());
		
		for (auto &line: response.lines) {
			int msg_id;
			Pdu pdu;
			SmsDir dir;
			PduUserDataHeader hdr;
			std::string decoded_text;
			size_t msg_hash;
			bool invalid = false;
			
			bool decode_success = false;
			if (decodeSmsToPdu(line, &dir, &pdu, &msg_id, &msg_hash)) {
				std::tie(decode_success, decoded_text) = decodeSmsDcsData(&pdu, &hdr);
				
				if (!decode_success)
					LOGE("Invalid PDU data in SMS: '%s'\n", line.c_str());
			}
			
			if (!decode_success) {
				hdr = {};
				decoded_text = "Invalid PDU:\n" + line;
				invalid = true;
			}
			
			if (hdr.app_port) {
				decoded_text = "Wireless Datagram Protocol\n"
					"Src port: " + std::to_string(hdr.app_port->src) + "\n"
					"Dst port: " + std::to_string(hdr.app_port->dst) + "\n"
					"Data: " + bin2hex(decoded_text) + "\n";
				invalid = true;
			}
			
			uint16_t ref_id = hdr.concatenated ? hdr.concatenated->ref_id : 0;
			uint8_t parts = hdr.concatenated ? hdr.concatenated->parts : 1;
			uint8_t part = hdr.concatenated ? hdr.concatenated->part : 1;
			
			if (part < 1 || part > parts) {
				LOGE("Invalid SMS part id: %d / %d, in: '%s'\n", part, parts, line.c_str());
				parts = 1;
				part = 1;
				ref_id = 0;
			}
			
			Sms *sms = nullptr;
			
			if (parts > 1) {
				if (pdu.type == PDU_TYPE_DELIVER) {
					sms_key = std::make_tuple(pdu.type, pdu.smsc.number, pdu.deliver().src.number, ref_id, parts);
				} else if (pdu.type == PDU_TYPE_SUBMIT) {
					sms_key = std::make_tuple(pdu.type, pdu.smsc.number, pdu.submit().dst.number, ref_id, parts);
				}
				
				if (sms_parts.find(sms_key) != sms_parts.cend()) {
					sms = &sms_list[sms_parts[sms_key]];
				} else {
					sms_parts[sms_key] = sms_list.size();
					sms_list.resize(sms_list.size() + 1);
					sms = &sms_list.back();
					sms->parts.resize(parts);
				}
			} else {
				sms_list.resize(sms_list.size() + 1);
				sms = &sms_list.back();
				sms->parts.resize(parts);
			}
			
			sms->id = msg_hash;
			sms->dir = dir;
			sms->unread = (dir == SMS_DIR_UNREAD);
			sms->invalid = invalid;
			
			switch (pdu.type) {
				case PDU_TYPE_DELIVER:
				{
					auto &deliver = pdu.deliver();
					sms->type = SMS_INCOMING;
					sms->time = deliver.dt.timestamp;
					sms->addr = deliver.src.number;
					
					if (deliver.src.type == PDU_ADDR_INTERNATIONAL) {
						sms->addr = "+" + deliver.src.number;
					} else {
						sms->addr = deliver.src.number;
					}
				}
				break;
				
				case PDU_TYPE_SUBMIT:
				{
					auto &submit = pdu.submit();
					sms->type = SMS_OUTGOING;
					sms->time = 0;
					
					if (submit.dst.type == PDU_ADDR_INTERNATIONAL) {
						sms->addr = "+" + submit.dst.number;
					} else {
						sms->addr = submit.dst.number;
					}
				}
				break;
			}
			
			sms->parts[part - 1].id = msg_id;
			sms->parts[part - 1].text = decoded_text;
		}
		auto elapsed = getCurrentTimestamp() - start;
		LOGD("Sms decode time: %d\n", static_cast<int>(elapsed));
		
		callback(true, sms_list);
	}, 0);
}

bool ModemBaseAt::deleteSms(int id) {
	return m_at.sendCommandNoResponse("AT+CMGD=" + std::to_string(id)) == 0;
}

/*
 * USSD
 * */
void ModemBaseAt::handleUssdResponse(int code, const std::string &data, int dcs) {
	auto [success, decoded] = decodeCbsDcsString(data, dcs);
	if (!success) {
		code = USSD_ERROR;
		decoded = "Can't decode USSD: " + bin2hex(data, true) + " [dcs=" + std::to_string(dcs) + "].";
		LOGE("%s\n", decoded.c_str());
	}
	
	if (m_ussd_callback) {
		uint32_t current_req = m_ussd_request_id;
		
		Loop::setTimeout([=]() {
			if (current_req == m_ussd_request_id) {
				auto callback = m_ussd_callback;
				Loop::clearTimeout(m_ussd_timeout);
				m_ussd_callback = nullptr;
				m_ussd_timeout = -1;
				m_ussd_session = (code == USSD_WAIT_REPLY);
				
				if (callback)
					callback(static_cast<UssdCode>(code), decoded);
			} else if (!isUssdBusy()) {
				LOGD("Unsolicited USSD: %s [code=%d]\n", decoded.c_str(), code);
				
				if (code == USSD_WAIT_REPLY)
					cancelUssd();
			}
		}, 0);
	}
}

void ModemBaseAt::handleCusd(const std::string &event) {
	int dcs = 0, code = 0;
	std::string data;
	
	bool success;
	int args_cnt = AtParser::getArgCnt(event);
	
	if (args_cnt == 1) {
		success = AtParser(event)
			.parseInt(&code)
			.success();
	} else {
		success = AtParser(event)
			.parseInt(&code)
			.parseString(&data)
			.parseInt(&dcs)
			.success();
	}
	
	if (!success) {
		LOGE("Invalid CUSD: %s\n", event.c_str());
		return;
	}
	
	// NOTE: hex2bin assume as raw data, if can't decode as hex
	handleUssdResponse(code, hex2bin(data), dcs);
}

bool ModemBaseAt::sendUssd(const std::string &cmd, UssdCallback callback, int timeout) {
	if (!timeout)
		timeout = getCommandTimeout("+CUSD");
	
	if (!isValidUssd(cmd)) {
		callback(USSD_ERROR, "Not valid ussd command.");
		return false;
	}
	
	if (isUssdBusy()) {
		if (!cancelUssd()) {
			callback(USSD_ERROR, "Previous USSD command not finished.");
			return false;
		}
	}
	
	m_ussd_request_id++;
	m_ussd_callback = callback;
	
	if (m_at.sendCommandNoResponse("AT+CUSD=1,\"" + cmd + "\",15") != 0) {
		m_ussd_callback = nullptr;
		return false;
	}
	
	m_ussd_timeout = Loop::setTimeout([=]() {
		m_ussd_callback = nullptr;
		callback(USSD_ERROR, "USSD command timeout reached.");
	}, timeout);
	
	return true;
}

bool ModemBaseAt::cancelUssd() {
	auto last_callback = m_ussd_callback;
	m_ussd_callback = nullptr;
	m_ussd_session = false;
	
	if (m_ussd_timeout != -1) {
		Loop::clearTimeout(m_ussd_timeout);
		m_ussd_timeout = -1;
	}
	
	if (last_callback) {
		Loop::setTimeout([=]() {
			last_callback(USSD_ERROR, "USSD command canceled.");
		}, 0);
	}
	
	return m_at.sendCommandNoResponse("AT+CUSD=2") == 0;
}

bool ModemBaseAt::isUssdBusy() {
	return m_ussd_callback != nullptr;
}

bool ModemBaseAt::isUssdWaitReply() {
	return m_ussd_session;
}

/*
 * SIM PIN
 * */
void ModemBaseAt::startSimPolling() {
	if (m_pin_state != PIN_UNKNOWN && m_pin_state != PIN_REQUIRED)
		return;
	
	auto old_state = m_pin_state;
	
	auto response = m_at.sendCommand("AT+CPIN?");
	if (response.isCmeError()) {
		int error = response.getCmeError();
		
		switch (error) {
			case 10:	// SIM not inserted
			case 13:	// SIM failure
			case 15:	// SIM wrong
				LOGD("SIM not present, no need PIN code\n");
				m_pin_state = PIN_NOT_SUPPORTED;
			break;
			
			case 14:	// SUM busy
				// SIM not ready, ignore this error
			break;
			
			default:
				LOGE("AT+CPIN command failed [CME ERROR %d]\n", error);
				m_pin_state = PIN_NOT_SUPPORTED;
			break;
		}
	} else if (response.error) {
		// AT+CPIN not supported
		m_pin_state = PIN_NOT_SUPPORTED;
	}
	
	if (old_state != m_pin_state) {
		emit<EvPinStateChaned>({.state = m_pin_state});
		
		if (m_pin_state != PIN_UNKNOWN && m_pin_state != PIN_REQUIRED)
			return;
	}
	
	Loop::setTimeout([=]() {
		startSimPolling();
	}, 1000);
}

void ModemBaseAt::handleCpin(const std::string &event) {
	std::string code;
	if (!AtParser(event).parseString(&code).success())
		return;
	
	auto old_state = m_pin_state;
	
	if (strStartsWith(code, "READY")) {
		m_pin_state = PIN_READY;
	} else if (strStartsWith(code, "SIM PIN")) {
		if (!m_pincode.size()) {
			LOGE("SIM pin required, but no pincode specified...\n");
			m_pin_state = PIN_ERROR;
		} else if (m_pincode_entered) {
			m_pin_state = PIN_ERROR;
		} else {
			m_pin_state = PIN_REQUIRED;
			
			// Trying enter PIN code only one time
			m_pincode_entered = true;
			
			Loop::setTimeout([=]() {
				if (m_at.sendCommandNoResponse("AT+CPIN=" + m_pincode) != 0)
					LOGE("SIM PIN unlock error\n");
				
				// Force request new status
				m_at.sendCommandNoResponse("AT+CPIN?");
			}, 0);
		}
	} else {
		LOGE("SIM required other lock code: %s\n", code.c_str());
		m_pin_state = PIN_ERROR;
	}
	
	if (old_state != m_pin_state)
		emit<EvPinStateChaned>({.state = m_pin_state});
}

/*
 * Network signal levels
 * */
void ModemBaseAt::handleCesq(const std::string &event) {
	static const double bit_errors[] = {0.14, 0.28, 0.57, 1.13, 2.26, 4.53, 9.05, 18.10};
	int rssi, ber, rscp, eclo, rsrq, rsrp;
	
	bool parsed = AtParser(event)
		.parseInt(&rssi)
		.parseInt(&ber)
		.parseInt(&rscp)
		.parseInt(&eclo)
		.parseInt(&rsrq)
		.parseInt(&rsrp)
		.success();
	
	if (!parsed)
		return;
	
	// RSSI (Received signal strength)
	m_levels.rssi_dbm = -(rssi >= 99 ? NAN : 111 - rssi);
	
	// Bit Error
	m_levels.bit_err_pct = ber >= 0 && ber < COUNT_OF(bit_errors) ? bit_errors[ber] : NAN;
	
	// RSCP (Received signal code power)
	m_levels.rscp_dbm = -(rscp >= 255 ? NAN : 121 - rscp);
	
	// Ec/lo
	m_levels.eclo_db = -(eclo >= 255 ? NAN : (49.0f - (float) eclo) / 2.0f);
	
	// RSRQ (Reference signal received quality)
	m_levels.rsrq_db = -(rsrq >= 255 ? NAN : (40.0f - (float) rsrq) / 2.0f);
	
	// RSRP (Reference signal received power)
	m_levels.rsrp_dbm = -(rsrp >= 255 ? NAN : 141 - rsrp);
	
	emit<EvSignalLevels>({});
}

/*
 * Read modem identification (model, vendor, sw version, IMEI, ...)
 * */
bool ModemBaseAt::readModemIdentification() {
	AtChannel::Response response;
	
	response = m_at.sendCommand("AT+CGMI", "+CGMI");
	if (response.error || !AtParser(response.data()).parseString(&m_hw_vendor).success())
		return false;
	
	response = m_at.sendCommand("AT+CGMM", "+CGMM");
	if (response.error || !AtParser(response.data()).parseString(&m_hw_model).success())
		return false;
	
	response = m_at.sendCommand("AT+CGMR", "+CGMR");
	if (response.error || !AtParser(response.data()).parseString(&m_sw_ver).success())
		return false;
	
	response = m_at.sendCommandNumericOrWithPrefix("AT+CGSN", "+CGSN");
	if (response.error) {
		// Some CDMA modems not support CGSN, but supports GSN
		response = m_at.sendCommandNumericOrWithPrefix("AT+GSN", "+GSN");
		if (response.error)
			return false;
	}
	
	if (!AtParser(response.data()).parseString(&m_imei).success())
		return false;
	
	return true;
}

bool ModemBaseAt::open() {
	// Try open serial
	if (m_serial.open(m_tty, m_speed) != 0) {
		LOGE("Can't open %s with speed %d...\n", m_tty.c_str(), m_speed);
		return false;
	}
	
	// Detect TTY device lost
	m_at.onIoBroken([=]() {
		Loop::setTimeout([=]() {
			emit<EvIoBroken>({});
			m_at.stop();
		}, 0);
	});
	
	// Detect modem hangs
	m_at.onAnyError([=](AtChannel::Errors error, int64_t start) {
		if (error == AtChannel::AT_IO_ERROR || error == AtChannel::AT_TIMEOUT) {
			if (!m_self_test) {
				m_self_test = true;
				LOGE("Detected IO error on AT channel... try ping for check...\n");
				if (!ping(32)) {
					LOGE("AT channel is broken!!!\n");
					emit<EvIoBroken>({});
					m_at.stop();
				}
				m_self_test = false;
			}
		}
	});
	
	// Start AT channel
	if (!m_at.start()) {
		LOGE("Can't start AT channel...\n");
		return false;
	}
	
	// Try handshake
	if (!handshake()) {
		LOGE("Can't handshake modem...\n");
		close();
		return false;
	}
	
	// Init modem
	if (!init()) {
		LOGE("Can't init modem...\n");
		close();
		return false;
	}
	
	return true;
}

void ModemBaseAt::close() {
	m_at.stop();
}
