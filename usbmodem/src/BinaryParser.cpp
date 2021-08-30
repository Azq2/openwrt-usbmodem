#include "BinaryParser.h"

bool BinaryParser::readByte(uint8_t *byte) {
	if (!m_data || m_offset >= m_size)
		return false;
	*byte = m_data[m_offset++];
	return true;
}

bool BinaryParser::readShortLE(uint16_t *value) {
	uint8_t data[2];
	if (!readByteArray(data, 2))
		return false;
	*value = (data[1] << 8) | data[0];
	return true;
}

bool BinaryParser::readShortBE(uint16_t *value) {
	uint8_t data[2];
	if (!readByteArray(data, 2))
		return false;
	*value = (data[0] << 8) | data[1];
	return true;
}

bool BinaryParser::readByteArray(uint8_t *data, size_t len) {
	if (m_offset + len > m_size)
		return false;
	memcpy(data, m_data + m_offset, len);
	m_offset += len;
	return true;
}

bool BinaryParser::readString(std::string *str, size_t len) {
	if (m_offset + len > m_size)
		return false;
	str->assign(m_str + m_offset, len);
	m_offset += len;
	return true;
}

bool BinaryParser::skip(size_t len) {
	if (m_offset + len > m_size)
		return false;
	m_offset += len;
	return true;
}

bool BinaryParser::truncate(size_t len) {
	if (m_size < len)
		return false;
	m_size = len;
	return true;
}
