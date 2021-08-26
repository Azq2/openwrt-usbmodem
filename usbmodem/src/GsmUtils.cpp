#include "GsmUtils.h"
#include "Log.h"

static constexpr uint16_t makeWideChar(uint8_t upper, uint8_t lower, bool be) {
	if (be) {
		return (upper << 8) | lower;
	} else {
		return (lower << 8) | upper;
	}
}

bool isValidUssd(const std::string &cmd) {
	if (!cmd.size())
		return false;
	
	for (auto c: cmd) {
		if (!isdigit(c) && c != '*' && c != '+' && c != '#')
			return false;
	}
	return true;
}

bool isValidLanguage(GsmLanguage lang) {
	switch (lang) {
		case GSM_LANG_GERMAN:		return true;
		case GSM_LANG_ENGLISH:		return true;
		case GSM_LANG_ITALIAN:		return true;
		case GSM_LANG_FENCH:		return true;
		case GSM_LANG_SPANISH:		return true;
		case GSM_LANG_DUTCH:		return true;
		case GSM_LANG_SWEDISH:		return true;
		case GSM_LANG_DANISH:		return true;
		case GSM_LANG_PORTUGUESE:	return true;
		case GSM_LANG_FINISH:		return true;
		case GSM_LANG_NORWEGIAN:	return true;
		case GSM_LANG_GREEK:		return true;
		case GSM_LANG_TURKISH:		return true;
		case GSM_LANG_HUNGARIAN:	return true;
		case GSM_LANG_POLISH:		return true;
		case GSM_LANG_UNSPEC:		return true;
		
		case GSM_LANG_CZECH:		return true;
		case GSM_LANG_HEBREW:		return true;
		case GSM_LANG_ARABIC:		return true;
		case GSM_LANG_RUSSIAN:		return true;
		case GSM_LANG_ICELANDIC:	return true;
	};
	return false;
}

bool decodeDcs(int dcs, GsmEncoding *out_encoding, GsmLanguage *out_language, bool *out_compression, bool *out_has_iso_lang) {
	GsmLanguage language = GSM_LANG_UNSPEC;
	GsmEncoding encoding = GSM_ENC_7BIT;
	bool compressed = false;
	bool has_iso_lang = false;
	
	uint8_t group = (dcs & 0xF0) >> 4;
	uint8_t value = (dcs & 0x0F);
	
	// CBS Data Coding Scheme
	switch (group) {
		case 0:
			encoding = GSM_ENC_7BIT;
			language = static_cast<GsmLanguage>(value);
		break;
		
		case 1:
			encoding = value ? GSM_ENC_UCS2 : GSM_ENC_7BIT;
			language = GSM_LANG_UNSPEC;
			has_iso_lang = true;
		break;
		
		case 2:
			encoding = GSM_ENC_7BIT;
			language = static_cast<GsmLanguage>(0x100 | value);
		break;
		
		case 3:
			encoding = GSM_ENC_7BIT;
			language = static_cast<GsmLanguage>(0x110 | value);
		break;
		
		case 4: case 5: case 6: case 7:
		{
			compressed = ((dcs & (1 << 5))) != 0;
			
			uint8_t charset_bits = (value & 0x0C) >> 2;
			if (charset_bits == 0) {
				encoding = GSM_ENC_7BIT;
			} else if (charset_bits == 1) {
				encoding = GSM_ENC_8BIT;
			} else if (charset_bits == 2) {
				encoding = GSM_ENC_UCS2;
			} else {
				// Invalid encoding
				return false;
			}
			
			language = GSM_LANG_UNSPEC;
		}
		break;
		
		case 15:
			if ((value & (1 << 2))) {
				encoding = GSM_ENC_8BIT;
			} else {
				encoding = GSM_ENC_7BIT;
			}
			
			language = GSM_LANG_UNSPEC;
		break;
		
		default:
			// Unknown dcs
			return false;
		break;
	}
	
	if (!isValidLanguage(language))
		language = GSM_LANG_UNSPEC;
	
	if (out_language)
		*out_language = language;
	
	if (out_encoding)
		*out_encoding = encoding;
	
	if (out_compression)
		*out_compression = compressed;
	
	if (out_has_iso_lang)
		*out_has_iso_lang = has_iso_lang;
	
	return true;
}

std::pair<bool, std::string> decodeDcsString(const std::string &data, int dcs) {
	GsmEncoding encoding;
	GsmLanguage language;
	bool compression;
	bool has_iso_lang;
	
	if (!decodeDcs(dcs, &encoding, &language, &compression, &has_iso_lang))
		return std::make_pair(false, "");
	
	LOGD("encoding=%d, language=%d, compression=%d, has_iso_lang=%d\n", encoding, language, compression, has_iso_lang);
	
	// Not supported for USSD
	if (compression || has_iso_lang)
		return std::make_pair(false, "");
	
	switch (encoding) {
		case GSM_ENC_7BIT:
		{
			bool success;
			std::string decoded;
			
			std::tie(success, decoded) = unpack7bit(data);
			if (success)
				return convertGsmToUtf8(decoded);
		}
		break;
		
		case GSM_ENC_8BIT:
			return convertGsmToUtf8(data);
		break;
		
		case GSM_ENC_UCS2:
			return convertUcs2ToUtf8(data, true);
		break;
	}
	
	return std::make_pair(false, "");
}

std::pair<bool, std::string> convertUcs2ToUtf8(const std::string &data, bool be) {
	if ((data.size() % 2) != 0) {
		// Invalid size
		return std::make_pair(false, "");
	}
	
	std::string out;
	out.reserve(data.size());
	
	for (size_t i = 0; i < data.size(); i += 2) {
		uint16_t W1 = makeWideChar(data[i], data[i + 1], be);
		uint32_t value;
		
		if (W1 >= 0xD800 && W1 <= 0xDBFF) {
			if (i + 3 >= data.size()) {
				// Unexpected EOF
				return std::make_pair(false, "");
			}
			
			uint32_t W2 = makeWideChar(data[i + 2], data[i + 3], be);
			if (W2 >= 0xDC00 && W2 <= 0xDFFF) {
				value = (((W1 - 0xD800) << 10) + (W2 - 0xDC00)) + 0x10000;
				i += 2;
			} else {
				// Invalid codepoint
				return std::make_pair(false, "");
			}
		} else {
			value = W1;
		}
		
		if (value < 0x80) {
			out += static_cast<char>(value);
		} else if (value < 0x800) {
			out += static_cast<char>((value >> 6) | 0xC0);
			out += static_cast<char>((value & 0x3F) | 0x80);
		} else if (value < 0xFFFF) {
			if (value >= 0xDC00 && value <= 0xDFFF) {
				// Invalid codepoint
				return std::make_pair(false, "");
			}
			
			out += static_cast<char>((value >> 12) | 0xE0);
			out += static_cast<char>(((value >> 6) & 0x3F) | 0x80);
			out += static_cast<char>((value & 0x3F) | 0x80);
		} else if (value < 0x10FFFF) {
			out += static_cast<char>((value >> 18) | 0xF0);
			out += static_cast<char>(((value >> 12) & 0x3F) | 0x80);
			out += static_cast<char>(((value >> 6) & 0x3F) | 0x80);
			out += static_cast<char>((value & 0x3F) | 0x80);
		} else {
			// Invalid codepoint
			return std::make_pair(false, "");
		}
    }
	
	return std::make_pair(true, out);
}

std::pair<bool, std::string> convertGsmToUtf8(const std::string &data) {
	
	return std::make_pair(true, data);
}

std::pair<bool, std::string> unpack7bit(const std::string &data) {
	return std::make_pair(true, data);
}
