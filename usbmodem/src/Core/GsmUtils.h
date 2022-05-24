#pragma once

#include <string>
#include <optional>
#include <tuple>
#include <any>

#include "BinaryStream.h"

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

enum GsmMessageClass {
	GSM_MSG_CLASS_0			= 0,
	GSM_MSG_CLASS_1			= 1,
	GSM_MSG_CLASS_2			= 2,
	GSM_MSG_CLASS_3			= 3,
	GSM_MSG_CLASS_UNSPEC	= 0xFF
};

enum PduAddrNumberingPlan: uint8_t {
	PDU_ADDR_PLAN_UNKNOWN	= 0,
	PDU_ADDR_PLAN_ISDN		= 1,
	PDU_ADDR_PLAN_DATA		= 3,
	PDU_ADDR_PLAN_TELEX		= 4,
	PDU_ADDR_PLAN_SC1		= 5,
	PDU_ADDR_PLAN_SC2		= 6,
	PDU_ADDR_PLAN_NATIONAL	= 8,
	PDU_ADDR_PLAN_PRIVATE	= 9,
	PDU_ADDR_PLAN_ERMES		= 10,
	PDU_ADDR_PLAN_RESERVED	= 15
};

enum PduAddrType: uint8_t {
	PDU_ADDR_UNKNOWN			= 0,
	PDU_ADDR_INTERNATIONAL		= 1,	
	PDU_ADDR_NATIONAL			= 2,
	PDU_ADDR_NETWORK_SPECIFIC	= 3,
	PDU_ADDR_SUBSCRIBER			= 4,
	PDU_ADDR_ALPHANUMERIC		= 5,
	PDU_ADDR_ABBREVIATED		= 6,
	PDU_ADDR_RESERVED			= 7,
};

enum PduValidityPeriodFormat: uint8_t {
	PDU_VPF_ABSENT		= 0,
	PDU_VPF_ENHANCED	= 1,
	PDU_VPF_RELATIVE	= 2,
	PDU_VPF_ABSOLUTE	= 3,
};

struct PduAddr {
	std::string number;
	PduAddrType type = PDU_ADDR_UNKNOWN;
	PduAddrNumberingPlan plan = PDU_ADDR_PLAN_UNKNOWN;
	
	inline std::string toString() {
		if (type == PDU_ADDR_INTERNATIONAL)
			return "+" + number;
		return number;
	}
};

enum PduType: uint8_t {
	PDU_TYPE_DELIVER		= 0,
	PDU_TYPE_SUBMIT_REPORT	= 1,
	PDU_TYPE_STATUS_REPORT	= 2,
	PDU_TYPE_RESERVED		= 3,
	PDU_TYPE_DELIVER_REPORT	= 4,
	PDU_TYPE_SUBMIT			= 5,
	PDU_TYPE_COMMAND		= 6,
	PDU_TYPE_UNKNOWN		= 0xFF
};

struct PduUserDataHeaderConcatenated {
	uint16_t ref_id;
	uint16_t parts;
	uint16_t part;
};

struct PduUserDataHeaderAppPort {
	uint16_t dst;
	uint16_t src;
};

struct PduUserDataHeader {
	std::optional<PduUserDataHeaderConcatenated> concatenated;
	std::optional<PduUserDataHeaderAppPort> app_port;
};

struct PduDateTime {
	time_t timestamp = 0;
	int tz = 0;
};

struct PduValidityPeriod {
	uint8_t relative;
	PduDateTime absolute;
	uint8_t enhanced[7];
};

struct PduDeliver {
	PduAddr src;
	PduDateTime dt;
	
	// TP-User-Data-Header-Indicator (TP-UDHI)
	bool udhi = false;
	
	// TP-More-Messages-to-Send (TP-MMS)
	bool mms = false;
	
	// TP-Loop-Prevention (TP-LP)
	bool lp = false;
	
	// TP-Status-Report-Indication (TP-SRI)
	bool sri = false;
	
	// TP-Reply-Path (TP-RP)
	bool rp = false;
	
	uint8_t pid = 0;
	uint8_t dcs = 0;
	uint8_t udl = 0;
	
	std::string data;
};

struct PduSubmit {
	PduAddr dst;
	PduValidityPeriod vp;
	
	// TP-User-Data-Header-Indicator (TP-UDHI)
	bool udhi = false;
	
	// TP-Reject-Duplicates (TP-RD)
	bool rd = false;
	
	// TP-Validity-Period-Format (TP-VPF)
	PduValidityPeriodFormat vpf = PDU_VPF_ABSENT;
	
	// TP-Reply-Path (TP-RP)
	bool rp = false;
	
	// TP-Status-Report-Request (TP-SRR)
	bool srr = false;
	
	uint8_t mr = 0;
	uint8_t pid = 0;
	uint8_t dcs = 0;
	uint8_t udl = 0;
	
	std::string data;
};

struct Pdu {
	PduAddr smsc;
	PduType type = PDU_TYPE_UNKNOWN;
	
	std::any payload;
	
	inline PduDeliver &deliver() {
		return std::any_cast<PduDeliver &>(payload);
	}
	
	inline const PduDeliver &deliver() const {
		return std::any_cast<const PduDeliver &>(payload);
	}
	
	inline PduSubmit &submit() {
		return std::any_cast<PduSubmit &>(payload);
	}
	
	inline const PduSubmit &submit() const {
		return std::any_cast<const PduSubmit &>(payload);
	}
};

constexpr size_t getPduMaxDataSize(PduType type) {
	switch (type) {
		case PDU_TYPE_DELIVER:		return 140;
		case PDU_TYPE_SUBMIT:		return 140;
	}
	return 0;
}

// PDU
bool decodePdu(const std::string &pdu_bytes, Pdu *pdu, bool direction_to_smsc);
bool decodePduAddr(BinaryBufferReader *parser, PduAddr *addr, bool is_smsc);
bool decodePduDateTime(BinaryBufferReader *parser, PduDateTime *dt);
bool decodePduValidityPeriodFormat(BinaryBufferReader *parser, PduValidityPeriodFormat vpf, PduValidityPeriod *vp);
bool decodePduDeliver(BinaryBufferReader *parser, Pdu *pdu, uint8_t flags);
bool decodePduSubmit(BinaryBufferReader *parser, Pdu *pdu, uint8_t flags);
size_t udlToBytes(uint8_t udl, int dcs);
int decodeUserDataHeader(const std::string &data, PduUserDataHeader *header);

// Data Coding
bool isValidLanguage(GsmLanguage lang);
bool decodeCbsDcs(int dcs, GsmEncoding *out_encoding, GsmLanguage *out_language, bool *out_compression, bool *out_has_iso_lang);
bool decodeSmsDcs(int dcs, GsmEncoding *out_encoding, bool *out_compression);
std::pair<bool, std::string> decodeCbsDcsString(const std::string &data, int dcs);
std::pair<bool, std::string> decodeSmsDcsData(const std::string &data, uint8_t udl, bool udhi, int dcs, PduUserDataHeader *header_out);
std::pair<bool, std::string> decodeSmsDcsData(Pdu *pdu, PduUserDataHeader *header_out);

// Ussd
bool isValidUssd(const std::string &cmd);

// Encodings
bool strAppendCodepoint(std::string &out, uint32_t value);
std::pair<bool, std::string> convertUcs2ToUtf8(const std::string &data, bool be);
std::string convertGsmToUtf8(const std::string &data);
std::string unpack7bit(const std::string &data, size_t max_chars);
inline std::string unpack7bit(const std::string &data) {
	return unpack7bit(data, data.size() * 8 / 7);
}
