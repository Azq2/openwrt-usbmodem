#include "BinaryStream.h"
#include "Utils.h"
#include "Log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * BinaryReaderBase
 * */
bool BinaryReaderBase::readPackedString(int size_type, std::string *str, bool be) {
	switch (size_type) {
		case 8:
		{
			uint8_t size8;
			if (!readUInt8(&size8))
				return false;
			return readString(str, size8);
		}
		break;
		
		case 16:
		{
			uint16_t size16;
			if (be) {
				if (!readUInt16BE(&size16))
					return false;
			} else {
				if (!readUInt16(&size16))
					return false;
			}
			return readString(str, size16);
		}
		break;
		
		case 32:
		{
			uint32_t size32;
			if (be) {
				if (!readUInt32BE(&size32))
					return false;
			} else {
				if (!readUInt32(&size32))
					return false;
			}
			return readString(str, size32);
		}
		break;
	}
	
	return false;
}

bool BinaryReaderBase::readString(std::string *str, size_t len) {
	if (!avail(len))
		return false;
	str->resize(len);
	return read(str->data(), len);
}

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

/*
 * BinaryWriterBase
 * */
bool BinaryWriterBase::writePackedString(int size_type, const std::string &str, bool be) {
	switch (size_type) {
		case 8:
			if (str.size() > 0xFF || !writeUInt8(str.size()))
				return false;
		break;
		
		case 16:
			if (be) {
				if (str.size() > 0xFFFF || !writeUInt16BE(str.size()))
					return false;
			} else {
				if (str.size() > 0xFFFF || !writeUInt16(str.size()))
					return false;
			}
		break;
		
		case 32:
			if (be) {
				if (str.size() > 0xFFFFFFFF || !writeUInt32BE(str.size()))
					return false;
			} else {
				if (str.size() > 0xFFFFFFFF || !writeUInt32(str.size()))
					return false;
			}
		break;
		
		default:
			// Unknown size
			return false;
		break;
	}
	return writeString(str);
}

bool BinaryWriterBase::writeUInt16(uint16_t value) {
	if (!isLittleEndian())
		value = __builtin_bswap16(value);
	return write(&value, 2);
}

bool BinaryWriterBase::writeUInt16BE(uint16_t value) {
	if (isLittleEndian())
		value = __builtin_bswap16(value);
	return write(&value, 2);
}

bool BinaryWriterBase::writeUInt32(uint32_t value) {
	if (!isLittleEndian())
		value = __builtin_bswap32(value);
	return write(&value, 4);
}

bool BinaryWriterBase::writeUInt32BE(uint32_t value) {
	if (isLittleEndian())
		value = __builtin_bswap32(value);
	return write(&value, 4);
}

bool BinaryWriterBase::writeUInt64(uint64_t value) {
	if (!isLittleEndian())
		value = __builtin_bswap64(value);
	return write(&value, 8);
}

bool BinaryWriterBase::writeUInt64BE(uint64_t value) {
	if (isLittleEndian())
		value = __builtin_bswap64(value);
	return write(&value, 8);
}

/*
 * BinaryBufferReader
 * */
bool BinaryBufferReader::avail(size_t size) {
	return m_data && m_offset + size <= m_size;
}

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
	if (!avail(len))
		return false;
	memcpy(data, m_data + m_offset, len);
	m_offset += len;
	return true;
}

bool BinaryBufferReader::skip(size_t len) {
	if (!avail(len))
		return false;
	m_offset += len;
	return true;
}

/*
 * BinaryFileReader
 * */
bool BinaryFileReader::setFile(FILE *fp) {
	struct stat st;
	int ret;
	
	if (!fp)
		return false;
	
	rewind(fp);
	
	do {
		ret = fstat(fileno(fp), &st);
	} while (ret < 0 && errno == EINTR);
	
	if (ret != 0)
		return false;
	
	m_fp = fp;
	m_size = st.st_size;
	
	return true;
}

bool BinaryFileReader::avail(size_t size) {
	return m_fp && offset() + size <= m_size;
}

size_t BinaryFileReader::size() {
	return m_size;
}

size_t BinaryFileReader::offset() {
	if (!m_fp)
		return 0;
	auto ret = ftell(m_fp);
	return ret < 0 ? std::string::npos : ret;
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
	uint8_t *data8 = reinterpret_cast<uint8_t *>(data);
	
	if (!avail(len))
		return false;
	
	size_t readed = 0;
	do {
		int ret = fread(data8 + readed, 1, len - readed, m_fp);
		if (ret < 0 && errno != EINTR) {
			LOGD("fread errno = %d\n", errno);
			return false;
		}
		
		if (ret >= 0)
			readed += ret;
	} while (readed < len);
	
}

bool BinaryFileReader::skip(size_t len) {
	if (!avail(len))
		return false;
	return fseek(m_fp, offset() + len, SEEK_SET) != 0;
}

/*
 * BinaryFileWriter
 * */
size_t BinaryBufferWriter::size() {
	return m_buffer.size();
}

size_t BinaryBufferWriter::offset() {
	return m_offset;
}

bool BinaryBufferWriter::write(const void *data, size_t len) {
	const uint8_t *data8 = reinterpret_cast<const uint8_t *>(data);
	m_buffer.insert(m_buffer.end(), data8, data8 + len);
	return true;
}

/*
 * BinaryFileWriter
 * */
bool BinaryFileWriter::setFile(FILE *fp) {
	if (!fp)
		return false;
	rewind(fp);
	m_fp = fp;
	return true;
}

size_t BinaryFileWriter::size() {
	struct stat st;
	int ret;
	
	do {
		ret = fstat(fileno(m_fp), &st);
	} while (ret < 0 && errno == EINTR);
	
	if (ret != 0)
		return 0;
	
	return st.st_size;
}

size_t BinaryFileWriter::offset() {
	if (!m_fp)
		return 0;
	
	long int ret;
	do {
		ret = ftell(m_fp);
	} while (ret < 0 && errno == EINTR);
	
	return ret < 0 ? std::string::npos : ret;
}

bool BinaryFileWriter::write(const void *data, size_t len) {
	const uint8_t *data8 = reinterpret_cast<const uint8_t *>(data);
	
	if (!m_fp)
		return false;
	
	size_t written = 0;
	do {
		int ret = fwrite(data8 + written, 1, len - written, m_fp);
		if (ret < 0 && errno != EINTR) {
			LOGD("fwrite errno = %d\n", errno);
			return false;
		}
		
		if (ret >= 0)
			written += ret;
	} while (written < len);
	
	return true;
}
