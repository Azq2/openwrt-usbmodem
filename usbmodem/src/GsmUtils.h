#pragma once

#include <string>
#include <optional>
#include <tuple>

enum GsmLanguage {
	GSM_LANG_GERMAN		= 0x00,
	GSM_LANG_ENGLISH	= 0x01,
	GSM_LANG_ITALIAN	= 0x02,
	GSM_LANG_FENCH		= 0x03,
	GSM_LANG_SPANISH	= 0x04,
	GSM_LANG_DUTCH		= 0x05,
	GSM_LANG_SWEDISH	= 0x06,
	GSM_LANG_DANISH		= 0x07,
	GSM_LANG_PORTUGUESE	= 0x08,
	GSM_LANG_FINISH		= 0x09,
	GSM_LANG_NORWEGIAN	= 0x0A,
	GSM_LANG_GREEK		= 0x0B,
	GSM_LANG_TURKISH	= 0x0C,
	GSM_LANG_HUNGARIAN	= 0x0D,
	GSM_LANG_POLISH		= 0x0E,
	GSM_LANG_UNSPEC		= 0x0F,
	
	GSM_LANG_CZECH		= 0x100,
	GSM_LANG_HEBREW		= 0x101,
	GSM_LANG_ARABIC		= 0x102,
	GSM_LANG_RUSSIAN	= 0x103,
	GSM_LANG_ICELANDIC	= 0x104,
};

enum GsmEncoding {
	GSM_ENC_7BIT		= 0,
	GSM_ENC_8BIT		= 1,
	GSM_ENC_UCS2		= 2
};

bool isValidLanguage(GsmLanguage lang);
bool decodeDcs(int dcs, GsmEncoding *out_encoding, GsmLanguage *out_language, bool *out_compression, bool *out_has_iso_lang);

bool isValidUssd(const std::string &cmd);

std::pair<bool, std::string> decodeDcsString(const std::string &data, int dcs);
std::pair<bool, std::string> convertUcs2ToUtf8(const std::string &data, bool be);
std::pair<bool, std::string> convertGsmToUtf8(const std::string &data);
std::pair<bool, std::string> unpack7bit(const std::string &data);
