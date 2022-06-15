#include "../BaseAt.h"

std::tuple<bool, BaseAtModem::ModemInfo> BaseAtModem::getModemInfo() {
	return cached<ModemInfo>(__func__, [this]() {
		AtChannel::Response response;
		ModemInfo info;
		
		response = m_at.sendCommandNoPrefix("AT+CGMI");
		info.vendor = response.error ? "" : AtParser::stripPrefix(response.data());
		
		response = m_at.sendCommandNoPrefix("AT+CGMM");
		info.model = response.error ? "" : AtParser::stripPrefix(response.data());
		
		response = m_at.sendCommandNoPrefix("AT+CGMR");
		info.version = response.error ? "" : AtParser::stripPrefix(response.data());
		
		response = m_at.sendCommandNumericOrWithPrefix("AT+CGSN", "+CGSN");
		if (response.error) {
			// Some CDMA modems not support CGSN, but supports GSN
			response = m_at.sendCommandNumericOrWithPrefix("AT+GSN", "+GSN");
		}
		info.imei = response.error ? "unknown" : response.data();
		
		return info;
	});
}
