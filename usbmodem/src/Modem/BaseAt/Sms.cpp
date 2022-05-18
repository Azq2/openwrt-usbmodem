#include "../BaseAt.h"

bool BaseAtModem::intiSms() {
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
	
	LOGD("SMS ready\n");
	
	m_sms_ready = true;
	
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

bool BaseAtModem::decodeSmsToPdu(const std::string &data, SmsDir *dir, Pdu *pdu, int *id, uint32_t *hash) {
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

std::tuple<bool, std::vector<BaseAtModem::Sms>> BaseAtModem::getSmsList(SmsDir from_dir) {
	if (from_dir > SMS_DIR_ALL || !m_sms_ready)
		return {false, {}};
	
	auto response = m_at.sendCommandMultiline("AT+CMGL=" + std::to_string(from_dir), "+CMGL");
	if (response.error)
		return {false, {}};
	
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
		uint32_t msg_hash;
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
		
		sms->hash = msg_hash;
		sms->dir = dir;
		sms->unread = (dir == SMS_DIR_UNREAD);
		sms->invalid = invalid;
		
		switch (pdu.type) {
			case PDU_TYPE_DELIVER:
			{
				auto &deliver = pdu.deliver();
				sms->type = SMS_INCOMING;
				sms->time = deliver.dt.timestamp;
				
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
	
	return {true, sms_list};
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
