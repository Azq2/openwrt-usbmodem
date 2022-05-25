#include "SmsDb.h"
#include "Log.h"

#include <unistd.h>
#include <sys/file.h>
#include <sys/statvfs.h>

void SmsDb::init() {
	m_inited = true;
}

const char *SmsDb::getStorageTypeName() {
	switch (m_storage_type) {
		case STORAGE_FILESYSTEM:		return "FILESYSTEM";
		case STORAGE_SIM:				return "SIM";
		case STORAGE_MODEM:				return "MODEM";
		case STORAGE_SIM_AND_MODEM:		return "SIM_AND_MODEM";
	}
	return "UNKNOWN";
}

int SmsDb::getUnreadCount() {
	int cnt = 0;
	for (auto &it: m_storage) {
		auto &sms = it.second;
		if ((sms.flags & SMS_IS_UNREAD))
			cnt++;
	}
	return cnt;
}

int SmsDb::getMaxCapacity() {
	if (m_storage_type == STORAGE_FILESYSTEM) {
		struct statvfs st;
		if (statvfs(m_db_filename.c_str(), &st) == 0) {
			size_t free_space = st.f_bsize * st.f_bavail;
			return std::min(static_cast<size_t>(10000), free_space / 0xFF / 2);
		}
		return 0;
	}
	return m_capacity;
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

void SmsDb::addToList(Sms *sms) {
	auto &list = m_list[sms->type];
	for (auto it = list.begin(); it != list.end(); it++) {
		if (sms->time >= m_storage[*it].time) {
			list.insert(it, sms->id);
			return;
		}
	}
	list.push_back(sms->id);
	
	m_used_capacity += sms->parts.size();
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
		
		addToList(sms);
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
		LOGE("Invalid db magic, expected %08X, but got %08X\n", magic, DB_MAGIC);
		return false;
	}
	
	if (!reader->readUInt8(&version) || version != DB_VERSION) {
		LOGE("Invalid db version, expected %d, but got %d\n", version, DB_VERSION);
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
		
		int sms_id = m_global_sms_id++;
		sms.id = sms_id;
		
		// Add sms to list
		m_storage[sms_id] = std::move(sms);
		addToList(&m_storage[sms_id]);
	}
	
	return true;
}

bool SmsDb::load() {
	m_list.clear();
	m_storage.clear();
	m_global_sms_id = 0;
	m_used_capacity = 0;
	
	if (!isFileExists(m_db_filename) || !getFileSize(m_db_filename))
		return true;
	
	FILE *fp = fopen(m_db_filename.c_str(), "r");
	if (!fp) {
		LOGE("Can't open '%s' for reading, errno = %d\n", m_db_filename.c_str(), errno);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_EX) != 0) {
		LOGE("Can't lock file '%s', errno = %d\n", m_db_filename.c_str(), errno);
		fclose(fp);
		return false;
	}
	
	BinaryFileReader reader(fp);
	if (!unserialize(&reader)) {
		LOGD("Can't unserialize SMS database from %s\n", m_db_filename.c_str());
		flock(fileno(fp), LOCK_UN);
		fclose(fp);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_UN) != 0) {
		LOGE("Can't unlock file '%s', errno = %d\n", m_db_filename.c_str(), errno);
		fclose(fp);
		return false;
	}
	
	fclose(fp);
	
	return true;
}

bool SmsDb::save() {
	FILE *fp = fopen(m_tmp_filename.c_str(), "w+");
	if (!fp) {
		LOGE("Can't open '%s' for writing, errno = %d\n", m_tmp_filename.c_str(), errno);
		return false;
	}
	
	if (flock(fileno(fp), LOCK_EX) != 0) {
		LOGE("Can't lock file '%s', errno = %d\n", m_tmp_filename.c_str(), errno);
		fclose(fp);
		unlink(m_tmp_filename.c_str());
		return false;
	}
	
	BinaryFileWriter writer(fp);
	if (!serialize(&writer)) {
		LOGD("Can't serialize SMS database to %s\n", m_tmp_filename.c_str());
		flock(fileno(fp), LOCK_UN);
		fclose(fp);
		unlink(m_tmp_filename.c_str());
		return false;
	}
	
	if (flock(fileno(fp), LOCK_UN) != 0) {
		LOGE("Can't unlock file '%s', errno = %d\n", m_tmp_filename.c_str(), errno);
		fclose(fp);
		unlink(m_tmp_filename.c_str());
		return false;
	}
	
	fclose(fp);
	
	if (rename(m_tmp_filename.c_str(), m_db_filename.c_str()) != 0) {
		unlink(m_tmp_filename.c_str());
		LOGD("Can't move '%s' to '%s', errno = %d\n", m_tmp_filename.c_str(), m_db_filename.c_str(), errno);
	}
	
	unlink(m_tmp_filename.c_str());
	
	return true;
}
