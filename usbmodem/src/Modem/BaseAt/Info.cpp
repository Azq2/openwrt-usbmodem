#include "../BaseAt.h"

std::tuple<bool, BaseAtModem::ModemInfo> BaseAtModem::getModemInfo() {
	return cached<ModemInfo>(__func__, [this]() {
		AtChannel::Response response;
		ModemInfo info;
		
		response = m_at.sendCommandNoPrefixAll("AT+CGMI");
		info.vendor = response.error ? "" : AtParser::stripPrefix(response.lines[response.lines.size() - 1]);
		
		response = m_at.sendCommandNoPrefixAll("AT+CGMM");
		info.model = response.error ? "" : AtParser::stripPrefix(response.lines[response.lines.size() - 1]);
		
		response = m_at.sendCommandNoPrefixAll("AT+CGMR");
		info.version = response.error ? "" : AtParser::stripPrefix(response.lines[response.lines.size() - 1]);
		
		response = m_at.sendCommandNumericOrWithPrefix("AT+CGSN", "+CGSN");
		if (response.error) {
			// Some CDMA modems not support CGSN, but supports GSN
			response = m_at.sendCommandNumericOrWithPrefix("AT+GSN", "+GSN");
		}
		info.imei = response.error ? "unknown" : AtParser::stripPrefix(response.data());
		
		return info;
	});
}
