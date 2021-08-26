#include "Modem.h"

Modem::Modem() {
	
}

Modem::~Modem() {
	
}

int Modem::getDelayAfterDhcpRelease() {
	return 0;
}

const char *Modem::getTechName(NetworkTech tech) {
	switch (tech) {
		case TECH_UNKNOWN:		return "unknown";
		case TECH_NO_SERVICE:	return "No service";
		case TECH_GSM:			return "GSM";
		case TECH_GPRS:			return "2G (GPRS)";
		case TECH_EDGE:			return "2G (EDGE)";
		case TECH_UMTS:			return "3G (UMTS)";
		case TECH_HSDPA:		return "3G (HSDPA)";
		case TECH_HSUPA:		return "3G (HSUPA)";
		case TECH_HSPA:			return "3G (HSPA)";
		case TECH_HSPAP:		return "3G (HSPA+)";
		case TECH_LTE:			return "4G (LTE)";
	}
	return "unknown";
}

const char *Modem::getNetRegStatusName(NetworkReg reg) {
	switch (reg) {
		case NET_NOT_REGISTERED:		return "not_registered";
		case NET_SEARCHING:				return "searching";
		case NET_REGISTERED_HOME:		return "home";
		case NET_REGISTERED_ROAMING:	return "roaming";
	}
	return "unknown";
}
