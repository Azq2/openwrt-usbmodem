#include "SmsDb.h"
#include "Log.h"

SmsDb::Sms *SmsDb::findSameSms(SmsType type, uint32_t ref_id, int part, int parts, const std::string addr, const std::string smsc) {
	if (parts < 2)
		return nullptr;
	
	for (auto &it: m_storage) {
		auto &sms = it.second;
		
		if (sms.type != type)
			continue;
		
		if (sms.ref_id != ref_id)
			continue;
		
		if (sms.parts.size() != parts)
			continue;
		
		if (sms.parts[part - 1].text.size() > 0)
			continue;
		
		if (sms.addr != addr)
			continue;
		
		if (sms.smsc != smsc)
			continue;
		
		return &sms;
	}
	
	return nullptr;
}

void SmsDb::loadRawPdu(SmsType type, int id, const std::string &hex) {
	Pdu pdu = {};
	PduUserDataHeader hdr = {};
	std::string text;
	bool decode_success = false;
	bool invalid = false;
	
	// Decode PDU
	if (decodePdu(hex2bin(hex), &pdu, (type == SMS_OUTGOING)))
		std::tie(decode_success, text) = decodeSmsDcsData(&pdu, &hdr);
	
	if (!decode_success) {
		text = "Invalid PDU: " + hex;
		invalid = true;
	}
	
	if (hdr.app_port) {
		text = "Wireless Datagram Protocol\n"
			"Src port: " + std::to_string(hdr.app_port->src) + "\n"
			"Dst port: " + std::to_string(hdr.app_port->dst) + "\n"
			"Data: " + bin2hex(text) + "\n"
			"Raw PDU: " + hex + "\n";
		invalid = true;
	}
	
	uint16_t ref_id = hdr.concatenated ? hdr.concatenated->ref_id : 0;
	uint8_t parts = hdr.concatenated ? hdr.concatenated->parts : 1;
	uint8_t part = hdr.concatenated ? hdr.concatenated->part : 1;
	
	if (part < 1 || part > parts) {
		LOGE("Invalid SMS part id: %d / %d, in: '%s'\n", part, parts, hex.c_str());
		parts = 1;
		part = 1;
		ref_id = 0;
	}
	
	time_t time = 0;
	std::string addr;
	std::string smsc = pdu.smsc.toString();
	
	if (pdu.type == PDU_TYPE_DELIVER) {
		time = pdu.deliver().dt.timestamp;
		addr = pdu.deliver().src.toString();
	} else if (pdu.type == PDU_TYPE_SUBMIT) {
		addr = pdu.submit().dst.toString();
	}
	
	Sms *sms = findSameSms(type, ref_id, part, parts, addr, smsc);
	if (!sms) {
		int sms_id = m_global_sms_id++;
		sms = &m_storage[sms_id];
		sms->id = sms_id;
		sms->type = type;
		sms->time = time;
		sms->addr = addr;
		sms->smsc = smsc;
		sms->ref_id = ref_id;
		sms->parts.resize(parts);
		
		m_used_capacity += parts;
		
		m_list[type].insert(m_list[type].begin(), sms_id);
	}
	
	sms->parts[part - 1].id = id;
	sms->parts[part - 1].text = text;
	
	sms->flags |= SMS_IS_UNREAD;
	
	if (invalid)
		sms->flags |= SMS_IS_INVALID;
}

std::vector<SmsDb::Sms> SmsDb::getSmsList(SmsType type, int offset, int limit) {
	auto &list = m_list[type];
	std::vector<Sms> result;
	for (auto i = offset; i < list.size(); i++) {
		if (i >= list.size())
			break;
		result.push_back(m_storage[list[i]]);
	}
	return result;
}

std::tuple<bool, SmsDb::Sms> SmsDb::getSmsById(int id) {
	if (m_storage.find(id) != m_storage.end())
		return {true, m_storage[id]};
	return {false, {}};
}

bool SmsDb::deleteSms(int id) {
	if (m_storage.find(id) == m_storage.end())
		return false;
	
	auto &sms = m_storage[id];
	
	m_used_capacity -= sms.parts.size();
	
	// Remove sms from device
	if (m_remove_sms_callback) {
		for (auto &part: sms.parts) {
			if (part.id != -1)
				m_remove_sms_callback(part.id);
		}
	}
	
	// Remove sms from lists
	auto it = std::find(m_list[sms.type].begin(), m_list[sms.type].end(), sms.id);
	if (it != m_list[sms.type].end())
		m_list[sms.type].erase(it);
	
	// Remove from storage
	m_storage.erase(id);
	
	return true;
}

bool SmsDb::save() {
	
	return true;
}
