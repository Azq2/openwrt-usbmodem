#include "SmsDb.h"
#include "Log.h"

#include <sys/file.h>
#include <unistd.h>

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

bool SmsDb::add(const RawSms &raw) {
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

bool SmsDb::add(const std::vector<RawSms> &list) {
	for (auto &raw: list) {
		if (!add(raw))
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

bool SmsDb::serialize(BinaryFileWriter *writer) {
	// Magic
	if (!writer->writeUInt32(DB_MAGIC))
		return false;
	
	// DB version
	if (!writer->writeUInt8(DB_VERSION))
		return false;
	
	for (auto &it: m_storage) {
		auto &sms = it.second;
		
		// Flags
		if (!writer->writeUInt8(sms.type))
			return false;
		if (!writer->writeUInt32(sms.flags))
			return false;
		if (!writer->writeUInt32(sms.ref_id))
			return false;
		if (!writer->writeUInt64(sms.time))
			return false;
		
		// Number and SMSC
		if (!writer->writePackedString(16, sms.addr))
			return false;
		if (!writer->writePackedString(16, sms.smsc))
			return false;
		
		// Number of parts
		if (!writer->writeUInt8(sms.parts.size()))
			return false;
		
		// Text of each parts
		for (auto &p: sms.parts) {
			if (!writer->writePackedString(16, p.text))
				return false;
		}
	}
	
	return true;
}

bool SmsDb::unserialize(BinaryFileReader *reader) {
	uint32_t magic = 0;
	uint8_t version = 0;
	
	if (!reader->readUInt32(&magic) || magic != DB_MAGIC) {
		LOGD("Invalid db magic, expected %08X, but got %08X\n", magic, DB_MAGIC);
		return false;
	}
	
	if (!reader->readUInt8(&version) || version != DB_VERSION) {
		LOGD("Invalid db version, expected %d, but got %d\n", version, DB_VERSION);
		return false;
	}
	
	while (!reader->eof()) {
		Sms sms;
		
		// Flags
		uint8_t type;
		if (!reader->readUInt8(&type))
			return false;
		sms.type = static_cast<SmsType>(type);
		
		uint32_t flags;
		if (!reader->readUInt32(&flags))
			return false;
		sms.flags = static_cast<SmsFlags>(flags);
		
		if (!reader->readUInt32(&sms.ref_id))
			return false;
		
		if (!reader->readUInt64(&sms.time))
			return false;
		
		// Number and SMSC
		if (!reader->readPackedString(16, &sms.addr))
			return false;
		if (!reader->readPackedString(16, &sms.smsc))
			return false;
		
		// Number of parts
		uint8_t parts_n;
		if (!reader->readUInt8(&parts_n))
			return false;
		sms.parts.resize(parts_n);
		
		// Text of each parts
		for (auto i = 0; i < parts_n; i++) {
			if (!reader->readPackedString(16, &sms.parts[i].text))
				return false;
		}
		
		// Add sms to list
		int sms_id = m_global_sms_id++;
		sms.id = sms_id;
		
		// TODO: sort by time
		m_list[sms.type].insert(m_list[sms.type].begin(), sms_id);
		m_storage[sms_id] = std::move(sms);
		
		m_used_capacity += parts_n;
	}
	
	return true;
}

bool SmsDb::load(const std::string &filename) {
	m_list.clear();
	m_storage.clear();
	m_global_sms_id = 0;
	m_used_capacity = 0;
	
	if (!isFileExists(filename) || !getFileSize(filename))
		return true;
	
	FILE *fp = fopen(filename.c_str(), "r");
	if (!fp) {
		LOGE("Can't open '%s' for reading, errno = %d\n", filename.c_str(), errno);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_EX) != 0) {
		LOGE("Can't lock file '%s', errno = %d\n", filename.c_str(), errno);
		fclose(fp);
		return false;
	}
	
	BinaryFileReader reader(fp);
	if (!unserialize(&reader)) {
		LOGD("Can't unserialize SMS database from %s\n", filename.c_str());
		flock(fileno(fp), LOCK_UN);
		fclose(fp);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_UN) != 0) {
		LOGE("Can't unlock file '%s', errno = %d\n", filename.c_str(), errno);
		fclose(fp);
		return false;
	}
	
	fclose(fp);
	
	return true;
}

bool SmsDb::save(const std::string &filename) {
	FILE *fp = fopen(filename.c_str(), "w+");
	if (!fp) {
		LOGE("Can't open '%s' for writing, errno = %d\n", filename.c_str(), errno);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_EX) != 0) {
		LOGE("Can't lock file '%s', errno = %d\n", filename.c_str(), errno);
		fclose(fp);
		unlink(filename.c_str());
		return false;
	}
	
	BinaryFileWriter writer(fp);
	if (!serialize(&writer)) {
		LOGD("Can't serialize SMS database to %s\n", filename.c_str());
		flock(fileno(fp), LOCK_UN);
		fclose(fp);
		unlink(filename.c_str());
		return false;
	}
	
	if (flock(fileno(fp), LOCK_UN) != 0) {
		LOGE("Can't unlock file '%s', errno = %d\n", filename.c_str(), errno);
		fclose(fp);
		unlink(filename.c_str());
		return false;
	}
	
	fclose(fp);
	
	return true;
}
