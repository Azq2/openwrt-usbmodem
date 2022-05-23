#pragma once

#include "Utils.h"
#include "GsmUtils.h"

#include <vector>
#include <functional>
#include <unordered_map>

class SmsDb {
	public:
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
			int id = -1;
			std::string text;
		};
		
		struct Sms {
			int id = 0;
			SmsType type = SMS_INCOMING;
			SmsFlags flags = SMS_NO_FLAGS;
			uint32_t ref_id = 0;
			time_t time = 0;
			std::string addr;
			std::string smsc;
			std::vector<SmsPart> parts;
		};
		
		typedef std::function<void(int id)> RemoveSmsCallback;
	protected:
		int m_capacity = 1000;
		int m_used_capacity = 0;
		int m_global_sms_id = 0;
		
		std::unordered_map<int, Sms> m_storage;
		std::unordered_map<SmsType, std::vector<int>> m_list = {
			{SMS_INCOMING, {}},
			{SMS_OUTGOING, {}},
			{SMS_DRAFT, {}},
		};
		
		RemoveSmsCallback m_remove_sms_callback;
		
		Sms *findSameSms(SmsType type, uint32_t ref_id, int part, int parts, const std::string addr, const std::string smsc);
	public:
		SmsDb() { }
		
		void loadRawPdu(SmsType type, int id, const std::string &hex);
		
		inline int getSmsCount(SmsType type) {
			return m_list[type].size();
		}
		
		inline void setMaxCapacity(int capacity) {
			m_capacity = capacity;
		}
		
		inline int getMaxCapacity() {
			return m_capacity;
		}
		
		inline int getUsedCapacity() {
			return m_used_capacity;
		}
		
		std::vector<Sms> getSmsList(SmsType type, int offset, int limit);
		
		std::tuple<bool, Sms> getSmsById(int id);
		
		bool deleteSms(int id);
		
		inline void setRemoveSmsCallback(const RemoveSmsCallback &callback) {
			m_remove_sms_callback = callback;
		}
		
		bool save();
};

DEFINE_ENUM_BIT_OPERATORS(SmsDb::SmsFlags);
