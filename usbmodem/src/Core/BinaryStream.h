#pragma once

#include <string>

class BinaryReaderBase {
	public:
		virtual size_t size() = 0;
		virtual size_t offset() = 0;
		virtual bool eof() = 0;
		virtual bool truncate(size_t len) = 0;
		virtual bool read(void *data, size_t len) = 0;
		virtual bool skip(size_t len) = 0;
		
		inline bool readByte(uint8_t *byte) {
			return read(byte, 1);
		}
		
		bool readUInt16(uint16_t *value);
		bool readUInt16BE(uint16_t *value);
		bool readUInt32(uint32_t *value);
		bool readUInt32BE(uint32_t *value);
		bool readUInt64(uint64_t *value);
		bool readUInt64BE(uint64_t *value);
		
		inline bool readInt16(int16_t *value) {
			return readUInt16(reinterpret_cast<uint16_t *>(value));
		}
		
		inline bool readInt16BE(int16_t *value) {
			return readUInt16BE(reinterpret_cast<uint16_t *>(value));
		}
		
		inline bool readInt32(int32_t *value) {
			return readUInt32(reinterpret_cast<uint32_t *>(value));
		}
		
		inline bool readInt32BE(int32_t *value) {
			return readUInt32BE(reinterpret_cast<uint32_t *>(value));
		}
		
		inline bool readInt64(int32_t *value) {
			return readUInt64(reinterpret_cast<uint64_t *>(value));
		}
		
		inline bool readInt64BE(int32_t *value) {
			return readUInt64BE(reinterpret_cast<uint64_t *>(value));
		}
		
		bool readString(std::string *str, size_t len);
};

class BinaryBufferReader: public BinaryReaderBase {
	protected:
		const uint8_t *m_data = nullptr;
		size_t m_size = 0;
		size_t m_offset = 0;
	public:
		BinaryBufferReader(const std::string &buffer) {
			setData(buffer);
		}
		
		BinaryBufferReader(const char *buffer, size_t size) {
			setData(buffer, size);
		}
		
		BinaryBufferReader(const uint8_t *buffer, size_t size) {
			setData(buffer, size);
		}
		
		inline void setData(const std::string &buffer) {
			setData(buffer.c_str(), buffer.size());
		}
		
		inline void setData(const char *buffer, size_t size) {
			setData(reinterpret_cast<const uint8_t *>(buffer), size);
		}
		
		inline void setData(const uint8_t *buffer, size_t size) {
			m_data = buffer;
			m_size = size;
			m_offset = 0;
		}
		
		size_t size() override;
		size_t offset() override;
		bool eof() override;
		bool truncate(size_t len) override;
		bool read(void *data, size_t len) override;
		bool skip(size_t len) override;
};

class BinaryFileReader: public BinaryReaderBase {
	protected:
		FILE *m_fp = nullptr;
		size_t m_size = 0;
	public:
		BinaryFileReader(FILE *fp) {
			setFile(fp);
		}
		
		bool setFile(FILE *fp);
		
		size_t size() override;
		size_t offset() override;
		bool eof() override;
		bool truncate(size_t len) override;
		bool read(void *data, size_t len) override;
		bool skip(size_t len) override;
};
