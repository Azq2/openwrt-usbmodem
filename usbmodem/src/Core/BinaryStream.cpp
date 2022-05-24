#include "BinaryStream.h"
#include "Utils.h"
#include "Log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * BinaryReaderBase
 * */
bool BinaryReaderBase::readUInt16(uint16_t *value) {
	if (!read(value, 2))
		return false;
	if (!isLittleEndian())
		*value = __builtin_bswap16(*value);
	return true;
}

bool BinaryReaderBase::readUInt16BE(uint16_t *value) {
	if (!read(value, 2))
		return false;
	if (isLittleEndian())
		*value = __builtin_bswap16(*value);
	return true;
}

bool BinaryReaderBase::readUInt32(uint32_t *value) {
	if (!read(value, 4))
		return false;
	if (!isLittleEndian())
		*value = __builtin_bswap32(*value);
	return true;
}

bool BinaryReaderBase::readUInt32BE(uint32_t *value) {
	if (!read(value, 4))
		return false;
	if (isLittleEndian())
		*value = __builtin_bswap32(*value);
	return true;
}

bool BinaryReaderBase::readUInt64(uint64_t *value) {
	if (!read(value, 8))
		return false;
	if (!isLittleEndian())
		*value = __builtin_bswap64(*value);
	return true;
}

bool BinaryReaderBase::readUInt64BE(uint64_t *value) {
	if (!read(value, 8))
		return false;
	if (isLittleEndian())
		*value = __builtin_bswap64(*value);
	return true;
}

bool BinaryReaderBase::readString(std::string *str, size_t len) {
	str->resize(len);
	return read(&(*str)[0], len);
}

/*
 * BinaryBufferReader
 * */
size_t BinaryBufferReader::size() {
	return m_size;
}

size_t BinaryBufferReader::offset() {
	return m_offset;
}

bool BinaryBufferReader::eof() {
	return m_offset >= m_size;
}

bool BinaryBufferReader::truncate(size_t len) {
	if (len > m_size)
		return false;
	m_size = len;
	return true;
}

bool BinaryBufferReader::read(void *data, size_t len) {
	if (!m_data || m_offset + len > m_size)
		return false;
	memcpy(data, m_data + m_offset, len);
	m_offset += len;
	return true;
}

bool BinaryBufferReader::skip(size_t len) {
	if (!m_data || m_offset + len > m_size)
		return false;
	m_offset += len;
	return true;
}

/*
 * BinaryFileReader
 * */
bool BinaryFileReader::setFile(FILE *fp) {
	if (!fp)
		return false;
	
	rewind(fp);
	
	struct stat st;
	if (fstat(fileno(fp), &st) != 0)
		return false;
	
	m_fp = fp;
	m_size = st.st_size;
	
	return true;
}

size_t BinaryFileReader::size() {
	return m_size;
}

size_t BinaryFileReader::offset() {
	if (!m_fp)
		return 0;
	auto ret = ftell(m_fp);
	return ret < 0 ? m_size : ret;
}

bool BinaryFileReader::eof() {
	return !m_fp || offset() >= m_size;
}

bool BinaryFileReader::truncate(size_t len) {
	if (!m_fp || len > m_size)
		return false;
	m_size = len;
	return true;
}

bool BinaryFileReader::read(void *data, size_t len) {
	if (!m_fp || offset() + len > m_size)
		return false;
	return fread(data, len, 1, m_fp) == 1;
}

bool BinaryFileReader::skip(size_t len) {
	if (!m_fp || offset() + len > m_size)
		return false;
	return fseek(m_fp, offset() + len, SEEK_SET) != 0;
}
