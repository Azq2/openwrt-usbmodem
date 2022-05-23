#include "../BaseAt.h"

bool BaseAtModem::intiSms() {
	if (m_sms_ready)
		return true;
	
	// Set PDU mode
	if (m_at.sendCommandNoResponse("AT+CMGF=0") != 0)
		return false;
	
	// Find best SMS storage
	if (!findBestSmsStorage(m_prefer_sms_to_sim))
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
	if (m_sms_all_storages[0].size() == 1 && m_sms_all_storages[0].size() == 1 && m_sms_all_storages[0].size() == 1) {
		m_sms_mem[0] = m_sms_all_storages[0][0];
		m_sms_mem[1] = m_sms_all_storages[1][0];
		m_sms_mem[2] = m_sms_all_storages[2][0];
		return true;
	}
	
	std::vector<SmsStorage> prefer_storages = {
		SMS_STORAGE_MT,
		SMS_STORAGE_ME,
		SMS_STORAGE_SM
	};
	
	if (prefer_sim)
		prefer_storages = {SMS_STORAGE_SM};
	
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

bool BaseAtModem::loadSmsToDb(SmsDb *sms, bool delete_from_storage) {
	auto response = m_at.sendCommandMultiline("AT+CMGL=" + std::to_string(SMS_DIR_ALL), "+CMGL");
	if (response.error)
		return false;
	
	auto start = getCurrentTimestamp();
	
	for (auto &line: response.lines) {
		int id, stat;
		std::string pdu_hex;
		
		bool success = AtParser(line)
			.parseInt(&id)
			.parseInt(&stat)
			.parseSkip()
			.parseSkip()
			.parseNewLine()
			.parseString(&pdu_hex)
			.success();
		
		if (!success)
			return false;
		
		SmsDb::SmsType type = (stat == SMS_DIR_SENT || stat == SMS_DIR_UNSENT ? SmsDb::SMS_OUTGOING : SmsDb::SMS_INCOMING);
		sms->loadRawPdu(type, id, pdu_hex);
	}
	
	auto elapsed = getCurrentTimestamp() - start;
	LOGD("Sms decode time: %d\n", static_cast<int>(elapsed));
	
	if (delete_from_storage) {
		// Delete all messages  except unread
		if (m_at.sendCommandNoResponse("AT+CMGD=0,3") != 0)
			return false;
	}
	
	return true;
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
