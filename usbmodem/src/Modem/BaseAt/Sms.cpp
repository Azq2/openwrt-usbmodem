#include "../BaseAt.h"

void BaseAtModem::handleCmt(const std::string &event) {
	if (strStartsWith(event, "+CMT:")) {
		
	} else if (strStartsWith(event, "+CMTI:")) {
		int index;
		bool success = AtParser(event)
			.parseSkip()
			.parseInt(&index)
			.success();
		if (!success)
			return;
		
		emit<EvNewStoredSms>({.index = index});
	}
}

bool BaseAtModem::intiSms() {
	if (m_sms_ready)
		return true;
	
	// Set PDU mode
	if (m_at.sendCommandNoResponse("AT+CMGF=0") != 0)
		return false;
	
	// Find best SMS storage
	bool prefer_sim = (m_sms_preferred_storage == SMS_PREFER_SIM);
	if (!findBestSmsStorage(prefer_sim))
		return false;
	
	// Set SMS storage
	if (!syncSmsStorage())
		return false;
	
	// Sync capacity
	if (!syncSmsCapacity())
		return false;
	
	m_sms_ready = true;
	
	emit<EvSmsReady>({});
	
	return true;
}

bool BaseAtModem::syncSmsStorage() {
	std::string cmd = "AT+CPMS=\"" + getSmsStorageName(m_sms_mem[0]) + "\",\"" + getSmsStorageName(m_sms_mem[1]) + "\",\"" + getSmsStorageName(m_sms_mem[2]) + "\"";
	return m_at.sendCommandNoResponse(cmd) == 0;
}

bool BaseAtModem::syncSmsCapacity() {
	auto response = m_at.sendCommand("AT+CPMS?", "+CPMS");
	if (response.error)
		return false;
	
	bool success = AtParser(response.data())
		.parseSkip()
		.parseInt(&m_sms_capacity[0].used)
		.parseInt(&m_sms_capacity[0].total)
		.parseSkip()
		.parseInt(&m_sms_capacity[1].used)
		.parseInt(&m_sms_capacity[1].total)
		.parseSkip()
		.parseInt(&m_sms_capacity[2].used)
		.parseInt(&m_sms_capacity[2].total)
		.success();
	
	return success;
}

bool BaseAtModem::isSmsStorageSupported(int mem_id, SmsStorage check_storage) {
	if (!discoverSmsStorages())
		return false;
	
	if (mem_id < 0 || mem_id >= 3)
		return false;
	
	for (auto &storage: m_sms_all_storages[mem_id]) {
		if (storage == check_storage)
			return true;
	}
	
	return false;
}

std::string BaseAtModem::getSmsStorageName(SmsStorage storage) {
	static std::map<SmsStorage, std::string> storage2name = {
		{SMS_STORAGE_SM, "SM"},
		{SMS_STORAGE_ME, "ME"},
		{SMS_STORAGE_MT, "MT"},
	};
	if (storage2name.find(storage) != storage2name.cend())
		return storage2name[storage];
	return "";
}

Modem::SmsStorage BaseAtModem::getSmsStorageId(const std::string &name) {
	static std::map<std::string, SmsStorage> name2storage = {
		{"SM", SMS_STORAGE_SM},
		{"ME", SMS_STORAGE_ME},
		{"MT", SMS_STORAGE_MT},
	};
	if (name2storage.find(name) != name2storage.cend())
		return name2storage[name];
	return SMS_STORAGE_UNKNOWN;
}

bool BaseAtModem::discoverSmsStorages() {
	if (m_storages_loaded)
		return true;
	
	auto response = m_at.sendCommand("AT+CPMS=?", "+CPMS");
	if (response.error)
		return false;
	
	std::vector<std::string> mem_names[3];
	
	bool success = AtParser(response.data())
		.parseArray(&mem_names[0])
		.parseArray(&mem_names[1])
		.parseArray(&mem_names[2])
		.success();
	
	if (!success)
		return false;
	
	for (int i = 0; i < 3; i++) {
		for (auto &name: mem_names[i]) {
			SmsStorage storage = getSmsStorageId(name);
			if (storage != SMS_STORAGE_UNKNOWN)
				m_sms_all_storages[i].push_back(storage);
		}
	}
	
	// Unknown modem
	if (!m_sms_all_storages[0].size() || !m_sms_all_storages[1].size() || !m_sms_all_storages[2].size()) {
		LOGE("Invalid SMS storages, CPMS: '%s'\n", response.data().c_str());
		return false;
	}
	
	m_storages_loaded = true;
	
	return true;
}

bool BaseAtModem::findBestSmsStorage(bool prefer_sim) {
	m_sms_mem[0] = SMS_STORAGE_UNKNOWN;
	m_sms_mem[1] = SMS_STORAGE_UNKNOWN;
	m_sms_mem[2] = SMS_STORAGE_UNKNOWN;
	
	if (!discoverSmsStorages())
		return false;
	
	// Nothing to choose
	if (m_sms_all_storages[0].size() == 1 && m_sms_all_storages[1].size() == 1 && m_sms_all_storages[2].size() == 1) {
		m_sms_mem[0] = m_sms_all_storages[0][0];
		m_sms_mem[1] = m_sms_all_storages[1][0];
		m_sms_mem[2] = m_sms_all_storages[2][0];
		return true;
	}
	
	if (isSmsStorageSupported(2, SMS_STORAGE_MT)) {
		if (isSmsStorageSupported(0, SMS_STORAGE_SM) && isSmsStorageSupported(1, SMS_STORAGE_SM)) {
			m_sms_mem[0] = SMS_STORAGE_SM;
			m_sms_mem[1] = SMS_STORAGE_SM;
			m_sms_mem[2] = SMS_STORAGE_MT;
		}
		
		if (!prefer_sim) {
			if (isSmsStorageSupported(0, SMS_STORAGE_ME) && isSmsStorageSupported(1, SMS_STORAGE_ME)) {
				m_sms_mem[0] = SMS_STORAGE_ME;
				m_sms_mem[1] = SMS_STORAGE_ME;
				m_sms_mem[2] = SMS_STORAGE_MT;
			}
		}
	}
	
	std::vector<SmsStorage> prefer_storages = {
		SMS_STORAGE_MT,
		SMS_STORAGE_ME,
		SMS_STORAGE_SM
	};
	
	if (prefer_sim)
		prefer_storages = {SMS_STORAGE_SM};
	
	for (auto &storage: prefer_storages) {
		if (isSmsStorageSupported(1, storage) && isSmsStorageSupported(2, storage)) {
			m_sms_mem[0] = isSmsStorageSupported(0, SMS_STORAGE_MT) ? SMS_STORAGE_MT : storage;
			m_sms_mem[1] = storage;
			m_sms_mem[2] = storage;
			break;
		}
	}
	
	bool success =
		m_sms_mem[0] != SMS_STORAGE_UNKNOWN &&
		m_sms_mem[1] != SMS_STORAGE_UNKNOWN &&
		m_sms_mem[2] != SMS_STORAGE_UNKNOWN;
	
	if (prefer_sim && !success)
		return findBestSmsStorage(false);
	
	return success;
}

bool BaseAtModem::decodePduToSms(SmsDb::SmsType type, SmsDb::RawSms *sms, const std::string &hex, int index, bool is_unread) {
	Pdu pdu = {};
	PduUserDataHeader hdr = {};
	bool decode_success = false;
	
	// Decode PDU
	if (decodePdu(hex2bin(hex), &pdu, (type == SmsDb::SMS_OUTGOING)))
		std::tie(decode_success, sms->text) = decodeSmsDcsData(&pdu, &hdr);
	
	if (!decode_success) {
		sms->text = "Invalid PDU: " + hex;
		sms->flags |= SmsDb::SMS_IS_INVALID;
	}
	
	if (hdr.app_port) {
		sms->text = "Wireless Datagram Protocol\n"
			"Src port: " + std::to_string(hdr.app_port->src) + "\n"
			"Dst port: " + std::to_string(hdr.app_port->dst) + "\n"
			"Data: " + bin2hex(sms->text) + "\n"
			"Raw PDU: " + hex + "\n";
		sms->flags |= SmsDb::SMS_IS_INVALID;
	}
	
	sms->ref_id = hdr.concatenated ? hdr.concatenated->ref_id : 0;
	sms->parts = hdr.concatenated ? hdr.concatenated->parts : 1;
	sms->part = hdr.concatenated ? hdr.concatenated->part : 1;
	
	if (sms->part < 1 || sms->part > sms->parts) {
		LOGE("Invalid SMS part id: %d / %d, in: '%s'\n", sms->part, sms->parts, hex.c_str());
		sms->parts = 1;
		sms->part = 1;
		sms->ref_id = 0;
	}
	
	sms->index = index;
	sms->type = type;
	sms->smsc = pdu.smsc.toString();
	sms->time = 0;
	
	if (pdu.type == PDU_TYPE_DELIVER) {
		sms->time = pdu.deliver().dt.timestamp;
		sms->addr = pdu.deliver().src.toString();
	} else if (pdu.type == PDU_TYPE_SUBMIT) {
		sms->addr = pdu.submit().dst.toString();
	}
	
	return true;
}

std::tuple<bool, std::vector<SmsDb::RawSms>> BaseAtModem::getSmsList(SmsListType list) {
	static std::map<SmsListType, SmsDir> list2dir = {
		{SMS_LIST_ALL, SMS_DIR_ALL},
		{SMS_LIST_UNREAD, SMS_DIR_UNREAD}
	};
	
	if (list2dir.find(list) == list2dir.end())
		return {false, {}};
	
	auto response = m_at.sendCommandMultiline("AT+CMGL=" + std::to_string(list2dir[list]), "+CMGL");
	if (response.error)
		return {false, {}};
	
	auto start = getCurrentTimestamp();
	
	std::vector<SmsDb::RawSms> result;
	
	for (auto &line: response.lines) {
		int index, stat;
		std::string pdu_hex;
		
		bool success = AtParser(line)
			.parseInt(&index)
			.parseInt(&stat)
			.parseSkip()
			.parseSkip()
			.parseNewLine()
			.parseString(&pdu_hex)
			.success();
		
		if (!success)
			return {false, {}};
		
		SmsDb::SmsType type = (stat == SMS_DIR_SENT || stat == SMS_DIR_UNSENT ? SmsDb::SMS_OUTGOING : SmsDb::SMS_INCOMING);
		bool is_unread = (stat == SMS_DIR_UNREAD);
		
		result.resize(result.size() + 1);
		SmsDb::RawSms &sms = result.back();
		
		if (!decodePduToSms(type, &sms, pdu_hex, index, is_unread))
			return {false, {}};
	}
	
	auto elapsed = getCurrentTimestamp() - start;
	LOGD("Sms decode time: %d\n", static_cast<int>(elapsed));
	
	return {true, result};
}

bool BaseAtModem::deleteReadedSms() {
	return m_at.sendCommandNoResponse("AT+CMGD=1,3") == 0;
}

bool BaseAtModem::deleteSms(int id) {
	if (!m_sms_ready)
		return false;
	return m_at.sendCommandNoResponse("AT+CMGD=" + std::to_string(id)) == 0;
}

BaseAtModem::SmsStorageCapacity BaseAtModem::getSmsCapacity() {
	return m_sms_capacity[2];
}

BaseAtModem::SmsStorage BaseAtModem::getSmsStorage() {
	return m_sms_mem[2];
}
