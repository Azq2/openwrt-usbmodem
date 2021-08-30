#pragma once

#include <string>

class BinaryParser {
	protected:
		const uint8_t *m_data = nullptr;
		const char *m_str = nullptr;
		size_t m_size = 0;
		size_t m_offset = 0;
	
	public:
		explicit BinaryParser(const std::string &s) {
			setData(s);
		}
		
		explicit BinaryParser(const uint8_t *s, size_t size) {
			setData(s, size);
		}
		
		BinaryParser() { }
		
		inline void setData(const std::string &s) {
			setData(reinterpret_cast<const uint8_t *>(s.c_str()), s.size());
		}
		
		inline void setData(const uint8_t *s, size_t size) {
			m_str = reinterpret_cast<const char *>(s);
			m_data = s;
			m_size = size;
			m_offset = 0;
		}
		
		inline size_t offset() const {
			return m_offset;
		}
		
		inline bool eof() const {
			return m_offset >= m_size;
		}
		
		bool truncate(size_t len);
		
		bool readByte(uint8_t *byte);
		bool readShortLE(uint16_t *value);
		bool readShortBE(uint16_t *value);
		bool readByteArray(uint8_t *data, size_t len);
		bool readString(std::string *str, size_t len);
		bool skip(size_t len);
};
