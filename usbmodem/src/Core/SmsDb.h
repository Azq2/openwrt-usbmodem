#pragma once

#include "Utils.h"
#include "GsmUtils.h"

#include <vector>
#include <functional>
#include <map>

class SmsDb {
	public:
		static constexpr uint8_t DB_VERSION = 0;
		static constexpr uint32_t DB_MAGIC = 0x534d53;
		
		enum StorageType {
			STORAGE_FILESYSTEM,
			STORAGE_SIM,
			STORAGE_MODEM,
			STORAGE_SIM_AND_MODEM
		};
		
		enum SmsType: uint8_t {
			SMS_INCOMING,
			SMS_OUTGOING,
			SMS_DRAFT
		};
		
		enum SmsFlags: uint32_t {
			SMS_NO_FLAGS	= 0,
			SMS_IS_UNREAD	= 1 << 0,
			SMS_IS_INVALID	= 1 << 1
		};
		
		struct SmsPart {
			int foreign_id = -1;
			std::string text;
		};
		
		struct Sms {
			int id = 0;
			SmsType type = SMS_INCOMING;
			SmsFlags flags = SMS_NO_FLAGS;
			uint32_t ref_id = 0;
			uint64_t time = 0;
			std::string addr;
			std::string smsc;
			std::vector<SmsPart> parts;
		};
		
		struct RawSms {
			int index = -1;
			SmsType type = SMS_INCOMING;
			SmsFlags flags = SMS_NO_FLAGS;
			int parts = 1;
			int part = 1;
			uint32_t ref_id = 0;
			uint64_t time = 0;
			std::string addr;
			std::string smsc;
			std::string text;
		};
		
		typedef std::function<void(int id)> RemoveSmsCallback;
	protected:
		int m_capacity = 1000;
		int m_used_capacity = 0;
		int m_global_sms_id = 0;
		bool m_inited = false;
		StorageType m_storage_type = STORAGE_FILESYSTEM;
		std::string m_db_filename = "/tmp/sms.dat";
		std::string m_tmp_filename = "/tmp/sms.dat.tmp";
		
		std::map<int, Sms, std::less<int>> m_storage;
		std::map<SmsType, std::vector<int>> m_list = {
			{SMS_INCOMING, {}},
			{SMS_OUTGOING, {}},
			{SMS_DRAFT, {}},
		};
		
		RemoveSmsCallback m_remove_sms_callback;
		
		Sms *findSameSms(const RawSms &raw);
		bool serialize(BinaryFileWriter *writer);
		bool unserialize(BinaryFileReader *reader);
		void addToList(Sms *sms);
	public:
		SmsDb() { }
		
		inline bool ready() {
			return m_inited;
		}
		
		void init();
		bool add(const RawSms &raw);
		bool add(const std::vector<RawSms> &list);
		
		int getUnreadCount();
		
		inline void setStorageType(StorageType storage) {
			m_storage_type = storage;
		}
		
		inline StorageType getStorageType() {
			return m_storage_type;
		}
		
		const char *getStorageTypeName();
		
		inline void setDbFile(const std::string &filename) {
			m_db_filename = filename;
		}
		
		inline void setTmpFile(const std::string &filename) {
			m_tmp_filename = filename;
		}
		
		inline int getSmsCount(SmsType type) {
			return m_list[type].size();
		}
		
		inline void setMaxCapacity(int capacity) {
			m_capacity = capacity;
		}
		
		int getMaxCapacity();
		
		inline int getUsedCapacity() {
			return m_used_capacity;
		}
		
		std::vector<Sms> getSmsList(SmsType type, int offset, int limit);
		
		std::tuple<bool, Sms> getSmsById(int id);
		
		bool deleteSms(int id);
		
		inline void setRemoveSmsCallback(const RemoveSmsCallback &callback) {
			m_remove_sms_callback = callback;
		}
		
		bool load();
		bool save();
};

DEFINE_ENUM_BIT_OPERATORS(SmsDb::SmsFlags);
