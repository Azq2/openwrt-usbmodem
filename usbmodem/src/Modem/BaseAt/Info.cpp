#include "../BaseAt.h"

std::tuple<bool, BaseAtModem::ModemInfo> BaseAtModem::getModemInfo() {
	return cached<ModemInfo>(__func__, [this]() {
		AtChannel::Response response;
		ModemInfo info;
		
		response = m_at.sendCommand("AT+CGMI", "+CGMI");
		
		if (response.error || !AtParser(response.data()).parseString(&info.vendor).success())
			info.vendor = "";
		
		response = m_at.sendCommand("AT+CGMM", "+CGMM");
		if (response.error || !AtParser(response.data()).parseString(&info.model).success())
			info.model = "";
		
		response = m_at.sendCommand("AT+CGMR", "+CGMR");
		if (response.error || !AtParser(response.data()).parseString(&info.version).success())
			info.version = "";
		
		response = m_at.sendCommandNumericOrWithPrefix("AT+CGSN", "+CGSN");
		if (response.error) {
			// Some CDMA modems not support CGSN, but supports GSN
			response = m_at.sendCommandNumericOrWithPrefix("AT+GSN", "+GSN");
		}
		
		if (response.error || !AtParser(response.data()).parseString(&info.imei).success())
			info.imei = "";
		
		return info;
	});
}
