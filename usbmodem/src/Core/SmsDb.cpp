#include "SmsDb.h"
#include "Log.h"

void SmsDb::init() {
	m_inited = true;
}

SmsDb::Sms *SmsDb::findSameSms(const RawSms &raw) {
	if (raw.parts < 2)
		return nullptr;
	
	for (auto &it: m_storage) {
		auto &sms = it.second;
		
		if (sms.type != raw.type)
			continue;
		
		if (sms.ref_id != raw.ref_id)
			continue;
		
		if (sms.parts.size() != raw.parts)
			continue;
		
		if (sms.parts[raw.part - 1].text.size() > 0)
			continue;
		
		if (sms.addr != raw.addr)
			continue;
		
		if (sms.smsc != raw.smsc)
			continue;
		
		return &sms;
	}
	
	return nullptr;
}

bool SmsDb::load(const RawSms &raw) {
	Sms *sms = findSameSms(raw);
	if (!sms) {
		int sms_id = m_global_sms_id++;
		sms = &m_storage[sms_id];
		sms->id = sms_id;
		sms->type = raw.type;
		sms->time = raw.time;
		sms->addr = raw.addr;
		sms->smsc = raw.smsc;
		sms->ref_id = raw.ref_id;
		sms->parts.resize(raw.parts);
		
		if (!sms->time)
			sms->time = time(nullptr);
		
		m_used_capacity += raw.parts;
		
		// TODO: sort by time
		m_list[raw.type].insert(m_list[raw.type].begin(), sms_id);
	}
	sms->parts[raw.part - 1].foreign_id = raw.index;
	sms->parts[raw.part - 1].text = raw.text;
	return true;
}

bool SmsDb::load(const std::vector<RawSms> &list) {
	for (auto &raw: list) {
		if (!load(raw))
			return false;
	}
	return true;
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
			if (part.foreign_id != -1)
				m_remove_sms_callback(part.foreign_id);
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
	
	
	int id = 0;
	SmsType type = SMS_INCOMING;
	SmsFlags flags = SMS_NO_FLAGS;
	uint32_t ref_id = 0;
	time_t time = 0;
	std::string addr;
	std::string smsc;
	std::vector<SmsPart> parts;
	
	return true;
}
