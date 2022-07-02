#include "ModemService.h"

#include <vector>
#include <csignal>
#include <Core/Uci.h>
#include <Core/UbusLoop.h>

void ModemService::loadSmsFromModem() {
	switch (m_sms_mode) {
		case SMS_MODE_MIRROR:
		{
			if (!m_sms.ready()) {
				if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_MT) {
					m_sms.setStorageType(SmsDb::STORAGE_SIM_AND_MODEM);
				} else if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_ME) {
					m_sms.setStorageType(SmsDb::STORAGE_MODEM);
				} else if (m_modem->getSmsStorage() == Modem::SMS_STORAGE_SM) {
					m_sms.setStorageType(SmsDb::STORAGE_SIM);
				} else {
					LOGE("Unknown storage type!\n");
					return;
				}
				
				auto capacity = m_modem->getSmsCapacity();
				m_sms.setMaxCapacity(capacity.total);
				
				m_sms.init();
				
				// Loading all messages to DB
				auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_ALL);
				if (!success || !m_sms.add(messages))
					LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			} else {
				// Loading unread messages to DB
				auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_UNREAD);
				if (!success || !m_sms.add(messages))
					LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			}
		}
		break;
		
		case SMS_MODE_DB:
		{
			if (!m_sms.ready()) {
				m_sms.setStorageType(SmsDb::STORAGE_FILESYSTEM);
				m_sms.init();
				
				if (!m_sms.load()) {
					LOGE("[sms] Failed to load sms database.\n");
				}
			}
			
			// Loading all messages to DB
			auto [success, messages] = m_modem->getSmsList(Modem::SMS_LIST_ALL);
			if (success && m_sms.add(messages)) {
				if (m_sms.save()) {
					// And now deleting all read SMS, because we need enough storage for new messages
					if (!m_modem->deleteReadedSms())
						LOGE("[sms] Failed to delete already readed SMS.\n");
				} else {
					LOGE("[sms] Failed to save sms database.\n");
				}
			} else {
				LOGE("[sms] Failed to load exists messages from sim/modem.\n");
			}
		}
		break;
	}
}
