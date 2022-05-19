#include "Modem.h"

const char *Modem::getEnumName(NetworkTech tech, bool is_human_readable) {
	if (is_human_readable) {
		switch (tech) {
			case TECH_UNKNOWN:		return "Unknown";
			case TECH_NO_SERVICE:	return "No service";
			case TECH_GSM:			return "GSM";
			case TECH_GSM_COMPACT:			return "GSM Compact";
			case TECH_GPRS:			return "2G (GPRS)";
			case TECH_EDGE:			return "2G (EDGE)";
			case TECH_UMTS:			return "3G (UMTS)";
			case TECH_HSDPA:		return "3G (HSDPA)";
			case TECH_HSUPA:		return "3G (HSUPA)";
			case TECH_HSPA:			return "3G (HSPA)";
			case TECH_HSPAP:		return "3G (HSPA+)";
			case TECH_LTE:			return "4G (LTE)";
		}
		return "Unknown";
	} else {
		switch (tech) {
			case TECH_UNKNOWN:		return "UNKNOWN";
			case TECH_NO_SERVICE:	return "NO_SERVICE";
			case TECH_GSM:			return "GSM";
			case TECH_GSM_COMPACT:			return "GSM_COMPACT";
			case TECH_GPRS:			return "GPRS";
			case TECH_EDGE:			return "EDGE";
			case TECH_UMTS:			return "UMTS";
			case TECH_HSDPA:		return "HSDPA";
			case TECH_HSUPA:		return "HSUPA";
			case TECH_HSPA:			return "HSPA";
			case TECH_HSPAP:		return "HSPAP";
			case TECH_LTE:			return "LTE";
		}
		return "UNKNOWN";
	}
}

const char *Modem::getEnumName(NetworkReg reg, bool is_human_readable) {
	if (is_human_readable) {
		switch (reg) {
			case NET_NOT_REGISTERED:		return "Unregistered from network";
			case NET_SEARCHING:				return "Searching network...";
			case NET_REGISTERED_HOME:		return "Registered to home network";
			case NET_REGISTERED_ROAMING:	return "Registered to roaming network";
		}
		return "UNKNOWN";
	} else {
		switch (reg) {
			case NET_NOT_REGISTERED:		return "NOT_REGISTERED";
			case NET_SEARCHING:				return "SEARCHING";
			case NET_REGISTERED_HOME:		return "REGISTERED_HOME";
			case NET_REGISTERED_ROAMING:	return "REGISTERED_ROAMING";
		}
		return "UNKNOWN";
	}
}

const char *Modem::getEnumName(SimState state, bool is_human_readable) {
	if (is_human_readable) {
		switch (state) {
			case SIM_NOT_INITIALIZED:	return "SIM not initialized";
			case SIM_NOT_SUPPORTED:		return "SIM not supported by Modem";
			case SIM_READY:				return "SIM is ready";
			case SIM_PIN1_LOCK:			return "SIM requires PIN1 code for unlock";
			case SIM_PIN2_LOCK:			return "SIM requires PIN2 code for unlock";
			case SIM_PUK1_LOCK:			return "SIM requires PUK1 code for unlock";
			case SIM_PUK2_LOCK:			return "SIM requires PUK2 code for unlock";
			case SIM_MEP_LOCK:			return "Modem requires MEP/NCK code for unlock";
			case SIM_OTHER_LOCK:		return "SIM or Modem requires other lock code";
			case SIM_WAIT_UNLOCK:		return "SIM unlocking...";
			case SIM_ERROR:				return "SIM error";
		}
		return "UNKNOWN";
	} else {
		switch (state) {
			case SIM_NOT_INITIALIZED:	return "NOT_INITIALIZED";
			case SIM_NOT_SUPPORTED:		return "NOT_SUPPORTED";
			case SIM_READY:				return "READY";
			case SIM_PIN1_LOCK:			return "PIN1_LOCK";
			case SIM_PIN2_LOCK:			return "PIN2_LOCK";
			case SIM_PUK1_LOCK:			return "PUK1_LOCK";
			case SIM_PUK2_LOCK:			return "PUK2_LOCK";
			case SIM_MEP_LOCK:			return "MEP_LOCK";
			case SIM_OTHER_LOCK:		return "OTHER_LOCK";
			case SIM_WAIT_UNLOCK:		return "WAIT_UNLOCK";
			case SIM_ERROR:				return "ERROR";
		}
		return "UNKNOWN";
	}
}

const char *Modem::getEnumName(OperatorRegMode state, bool is_human_readable) {
	if (is_human_readable) {
		switch (state) {
			case OPERATOR_REG_NONE:		return "none";
			case OPERATOR_REG_AUTO:		return "auto";
			case OPERATOR_REG_MANUAL:	return "manual";
		}
		return "unknown";
	} else {
		switch (state) {
			case OPERATOR_REG_NONE:		return "NONE";
			case OPERATOR_REG_AUTO:		return "AUTO";
			case OPERATOR_REG_MANUAL:	return "MANUAL";
		}
		return "UNKNOWN";
	}
}

const char *Modem::getEnumName(OperatorStatus state, bool is_human_readable) {
	if (is_human_readable) {
		switch (state) {
			case OPERATOR_STATUS_UNKNOWN:		return "unknown";
			case OPERATOR_STATUS_AVAILABLE:		return "available";
			case OPERATOR_STATUS_REGISTERED:	return "registered";
			case OPERATOR_STATUS_FORBIDDEN:		return "forbidden";
		}
		return "unknown";
	} else {
		switch (state) {
			case OPERATOR_STATUS_UNKNOWN:		return "UNKNOWN";
			case OPERATOR_STATUS_AVAILABLE:		return "AVAILABLE";
			case OPERATOR_STATUS_REGISTERED:	return "REGISTERED";
			case OPERATOR_STATUS_FORBIDDEN:		return "FORBIDDEN";
		}
		return "UNKNOWN";
	}
}

std::tuple<bool, std::any> Modem::cached(const std::string &key, std::function<std::any()> callback, int version) {
	if (m_cache.find(key) == m_cache.end() || m_cache[key].version != version) {
		auto value = callback();
		if (value.type() == typeid(nullptr) || value.type() == typeid(void))
			return {false, {}};
		m_cache[key] = {value, version};
	}
	return {true, m_cache[key].value};
}
