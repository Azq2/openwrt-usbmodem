#include "GsmUtils.h"
#include "Log.h"
#include "Utils.h"
#include <map>

// Default GSM 7bit charset
static const uint16_t GSM7_TO_UNICODE[] = {
	0x0040, 0x00A3, 0x0024, 0x00A5, 0x00E8, 0x00E9, 0x00F9, 0x00EC, 0x00F2, 0x00C7, 0x000A, 0x00D8, 0x00F8, 0x000D, 0x00C5, 0x00E5,
	0x0394, 0x005F, 0x03A6, 0x0393, 0x039B, 0x03A9, 0x03A0, 0x03A8, 0x03A3, 0x0398, 0x039E, 0x00A0, 0x00C6, 0x00E6, 0x00DF, 0x00C9,
	0x0020, 0x0021, 0x0022, 0x0023, 0x00A4, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
	0x00A1, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x00C4, 0x00D6, 0x00D1, 0x00DC, 0x00A7,
	0x00BF, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x00E4, 0x00F6, 0x00F1, 0x00FC, 0x00E0
};

// Default GSM 7bit charset (extended)
static const std::map<uint8_t, uint16_t> GSM7_TO_UNICODE_EXT = {
	{0x0A, 0x000C}, {0x14, 0x005E}, {0x1B, 0x0020}, {0x28, 0x007B},
	{0x29, 0x007D}, {0x2F, 0x005C}, {0x3C, 0x005B}, {0x3D, 0x007E},
	{0x3E, 0x005D}, {0x40, 0x007C}, {0x65, 0x20AC}
};

static constexpr uint8_t decodeDateField(uint8_t value) {
	return ((value & 0xF) * 10) + (value >> 4);
}

bool decodePduAddr(BinaryParser *parser, PduAddr *addr, bool is_smsc) {
	uint8_t addr_len;
	if (!parser->readByte(&addr_len))
		return false;
	
	if (!addr_len) {
		addr->plan = PDU_ADDR_PLAN_UNKNOWN;
		addr->type = PDU_ADDR_UNKNOWN;
		addr->number = "";
		return true;
	}
	
	uint8_t addr_type;
	if (!parser->readByte(&addr_type))
		return false;
	
	addr->plan = static_cast<PduAddrNumberingPlan>(addr_type & 0xF);
	addr->type = static_cast<PduAddrType>((addr_type >> 4) & 0x7);
	
	// When is_smsc=true mean total bytes for address + one type byte
	// When is_smsc=false mean total semi-octets (4 bit) in address
	uint8_t byte_len = is_smsc ? addr_len - 1 : (addr_len + 1) / 2;
	
	std::string raw_number;
	if (byte_len > 0 && !parser->readString(&raw_number, byte_len))
		return false;
	
	if (addr->type == PDU_ADDR_ALPHANUMERIC) {
		uint8_t chars_n = is_smsc ? byte_len * 8 / 7 : addr_len * 4 / 7;
		addr->number = convertGsmToUtf8(unpack7bit(raw_number, chars_n));
	} else {
		addr->number = decodeBcd(raw_number);
	}
	
	return true;
}

bool decodePduDateTime(BinaryParser *parser, PduDateTime *dt) {
	uint8_t year, month, day, hour, minute, second, raw_rz;
	
	if (!parser->readByte(&year))
		return false;
	year = decodeDateField(year);
	
	if (!parser->readByte(&month))
		return false;
	month = decodeDateField(month);
	
	if (!parser->readByte(&day))
		return false;
	day = decodeDateField(day);
	
	if (!parser->readByte(&hour))
		return false;
	hour = decodeDateField(hour);
	
	if (!parser->readByte(&minute))
		return false;
	minute = decodeDateField(minute);
	
	if (!parser->readByte(&second))
		return false;
	second = decodeDateField(second);
	
	if (!parser->readByte(&raw_rz))
		return false;
	
	int timezone = ((raw_rz & 0x7) * 10 + (raw_rz >> 4)) * 15 * 60;
	if ((raw_rz & (1 << 3)))
		timezone = -timezone;
	
	if (year > 99 || month > 12 || month < 1 || day > 31 || day < 1 || hour > 23 || minute > 59 || second > 59)
		return false;
	
	if (timezone > 14 * 3600 || timezone < -12 * 3600)
		return false;
	
	struct tm sms_time = {};
	sms_time.tm_year = year > 80 ? year : 100 + year;
	sms_time.tm_mon = month - 1;
	sms_time.tm_mday = day;
	sms_time.tm_hour = hour;
	sms_time.tm_min = minute;
	sms_time.tm_sec = second;
	
	dt->tz = timezone;
	dt->timestamp = timegm(&sms_time) - timezone;
	
	return true;
}

bool decodePduValidityPeriodFormat(BinaryParser *parser, PduValidityPeriodFormat vpf, PduValidityPeriod *vp) {
	switch (vpf) {
		case PDU_VPF_ABSENT:
			// None
			return true;
		break;
		
		case PDU_VPF_ENHANCED:
			if (!parser->readByteArray(vp->enhanced, 7))
				return false;
			return true;
		break;
		
		case PDU_VPF_RELATIVE:
			if (!parser->readByte(&vp->relative))
				return false;
			return true;
		break;
		
		case PDU_VPF_ABSOLUTE:
			if (!decodePduDateTime(parser, &vp->absolute))
				return false;
			return true;
		break;
	}
	return false;
}

size_t udlToBytes(uint8_t udl, uint8_t dcs) {
	size_t len_7bit = (udl + 1) * 7 / 8;
	size_t len_8bit = udl;
	
	// Zero dcs mean "GSM 7 bit default alphabet"
	if (!dcs)
		return len_7bit;
	
	uint8_t group = (dcs & 0xF0) >> 4;
	uint8_t value = (dcs & 0x0F);
	
	// SMS Data Coding Scheme
	switch (group) {
		case 0: case 1: case 2: case 3: // 00xx
		case 4: case 5: case 6: case 7: // 01xx
		{
			// Compression
			if ((group & 1))
				return len_8bit;
			
			uint8_t encoding = (value >> 2) & 0x3;
			if (encoding == 0) {
				// GSM 7 bit default alphabet
				return len_7bit;
			} else if (encoding == 1) {
				// 8 bit data
				return len_8bit;
			} else if (encoding == 2) {
				// CUS2
				return len_8bit;
			}
			
			// Unknown
			return 0;
		}
		break;
		
		case 12: // 1100
		case 13: // 1101
			return len_7bit; // GSM 7 bit default alphabet
		break;
		
		case 14: // 1110
			return len_8bit; // UCS2
		break;
		
		case 15: // 1111
			if ((value & (1 << 2))) {
				// 8 bit data
				return len_8bit;
			} else {
				// GSM 7 bit default alphabet
				return len_8bit;
			}
		break;
	}
	
	return 0;
}

bool decodePduDeliver(BinaryParser *parser, Pdu *pdu, uint8_t flags) {
	auto &deliver = pdu->deliver();
	
	deliver.mms = (flags & (1 << 2)) == 0;
	deliver.lp = (flags & (1 << 3)) != 0;
	deliver.sri = (flags & (1 << 5)) != 0;
	deliver.udhi = (flags & (1 << 6)) != 0;
	deliver.rp = (flags & (1 << 7)) != 0;
	
	// Sender address
	if (!decodePduAddr(parser, &deliver.src, false))
		return false;
	
	// Protocol ID
	if (!parser->readByte(&deliver.pid))
		return false;
	
	// Data Coding Scheme
	if (!parser->readByte(&deliver.dcs))
		return false;
	
	// SMSC timestamp
	if (!decodePduDateTime(parser, &deliver.dt))
		return false;
	
	// User data length
	if (!parser->readByte(&deliver.udl))
		return false;
	
	size_t udl_bytes = udlToBytes(deliver.udl, deliver.dcs);
	if (udl_bytes > getPduMaxDataSize(pdu->type))
		return false;
	
	// User Data
	if (!parser->readString(&deliver.data, udl_bytes))
		return false;
	
	return true;
}

bool decodePduSubmit(BinaryParser *parser, Pdu *pdu, uint8_t flags) {
	auto &submit = pdu->submit();
	
	submit.rd = (flags & (1 << 2)) != 0;
	submit.vpf = static_cast<PduValidityPeriodFormat>((flags >> 3) & 0x3);
	submit.rp = (flags & (1 << 7)) != 0;
	submit.udhi = (flags & (1 << 6)) != 0;
	submit.srr = (flags & (1 << 5)) != 0;
	
	// Message Reference
	if (!parser->readByte(&submit.mr))
		return false;
	
	// Receiver address
	if (!decodePduAddr(parser, &submit.dst, false))
		return false;
	
	// Protocol ID
	if (!parser->readByte(&submit.pid))
		return false;
	
	// Data Coding Scheme
	if (!parser->readByte(&submit.dcs))
		return false;
	
	// Validity Period
	if (!decodePduValidityPeriodFormat(parser, submit.vpf, &submit.vp))
		return false;
	
	// User data length
	if (!parser->readByte(&submit.udl))
		return false;
	
	size_t udl_bytes = udlToBytes(submit.udl, submit.dcs);
	if (udl_bytes > getPduMaxDataSize(pdu->type))
		return false;
	
	// User Data
	if (!parser->readString(&submit.data, udl_bytes))
		return false;
	
	return true;
}

// https://en.wikipedia.org/wiki/GSM_03.40
bool decodePdu(const std::string &pdu_bytes, Pdu *pdu, bool direction_to_smsc) {
	BinaryParser parser(pdu_bytes);
	
	// SMSC
	if (!decodePduAddr(&parser, &pdu->smsc, true))
		return false;
	
	// PDU type
	uint8_t flags;
	if (!parser.readByte(&flags))
		return false;
	
	if (direction_to_smsc) {
		pdu->type = static_cast<PduType>((flags & 0x3) | (1 << 2));
	} else {
		pdu->type = static_cast<PduType>(flags & 0x3);
	}
	
	switch (pdu->type) {
		case PDU_TYPE_DELIVER:
			pdu->payload.emplace<PduDeliver>();
			return decodePduDeliver(&parser, pdu, flags);
		break;
		
		case PDU_TYPE_SUBMIT:
			pdu->payload.emplace<PduSubmit>();
			return decodePduSubmit(&parser, pdu, flags);
		break;
	}
	
	return false;
}

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

bool decodeCbsDcs(int dcs, GsmEncoding *out_encoding, GsmLanguage *out_language, bool *out_compression, bool *out_has_iso_lang) {
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

std::pair<bool, std::string> decodeCbsDcsString(const std::string &data, int dcs) {
	GsmEncoding encoding;
	GsmLanguage language;
	bool compression;
	bool has_iso_lang;
	
	if (!decodeCbsDcs(dcs, &encoding, &language, &compression, &has_iso_lang))
		return std::make_pair(false, "");
	
	// Not supported for USSD
	if (compression || has_iso_lang)
		return std::make_pair(false, "");
	
	switch (encoding) {
		case GSM_ENC_7BIT:
			return std::make_pair(true, convertGsmToUtf8(unpack7bit(data)));
		break;
		
		case GSM_ENC_8BIT:
			return std::make_pair(true, convertGsmToUtf8(data));
		break;
		
		case GSM_ENC_UCS2:
			return convertUcs2ToUtf8(data, true);
		break;
	}
	
	return std::make_pair(false, "");
}

bool strAppendCodepoint(std::string &out, uint32_t value) {
	if (value < 0x80) {
		out += static_cast<char>(value);
	} else if (value < 0x800) {
		out += static_cast<char>((value >> 6) | 0xC0);
		out += static_cast<char>((value & 0x3F) | 0x80);
	} else if (value < 0xFFFF) {
		if (value >= 0xDC00 && value <= 0xDFFF) {
			// Invalid codepoint
			return false;
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
		return false;
	}
	
	return true;
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
		
		if (!strAppendCodepoint(out, value))
			return std::make_pair(false, "");
    }
	
	return std::make_pair(true, out);
}

std::string convertGsmToUtf8(const std::string &data) {
	std::string out;
	out.reserve(data.size());
	
	bool escape = false;
	for (auto c: data) {
		uint8_t byte = static_cast<uint8_t>(c);
		if (escape) {
			auto found = GSM7_TO_UNICODE_EXT.find(byte);
			if (found != GSM7_TO_UNICODE_EXT.cend()) {
				strAppendCodepoint(out, found->second);
			} else {
				out += ' ';
				strAppendCodepoint(out, GSM7_TO_UNICODE[byte]);
			}
			escape = false;
		} else {
			if (byte > 0x7F) {
				// Impossible worst case
				// Replacement Character for any invalid GSM7
				strAppendCodepoint(out, 0xFFFD);
			} else if (byte == 0x1B) {
				escape = true;
			} else {
				strAppendCodepoint(out, GSM7_TO_UNICODE[byte]);
			}
		}
	}
	
	if (escape)
		out += ' ';
	
	return out;
}

std::string unpack7bit(const std::string &data, size_t max_chars) {
	size_t total_chars = std::min(max_chars, data.size() * 8 / 7);
	
	std::string out;
	out.reserve(total_chars);
	
	const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data.c_str());
	for (size_t i = 0; i < total_chars; i++) {
		size_t bit_off = i * 7;
		size_t byte_off = bit_off / 8;
		
		uint8_t shift = bit_off % 8;
		uint8_t size = std::min(7, 8 - shift);
		
		uint8_t value = (bytes[byte_off] >> shift) & ((1 << size) - 1);
		
		if (size < 7)
			value |= (bytes[byte_off + 1] & ((1 << (7 - size)) - 1)) << size;
		
		out += static_cast<char>(value);
	}
	
	return out;
}
