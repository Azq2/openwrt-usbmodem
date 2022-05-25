#pragma once

#include <string>
#include <vector>

class BinaryReaderBase {
	public:
		virtual size_t size() = 0;
		virtual size_t offset() = 0;
		virtual bool eof() = 0;
		virtual bool truncate(size_t len) = 0;
		virtual bool read(void *data, size_t len) = 0;
		virtual bool skip(size_t len) = 0;
		virtual bool avail(size_t size) = 0;
		
		bool readString(std::string *str, size_t len);
		bool readPackedString(int size_type, std::string *str, bool be = false);
		
		inline bool readUInt8(uint8_t *byte) {
			return read(byte, 1);
		}
		
		inline bool readInt8(int8_t *byte) {
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
};

class BinaryWriterBase {
	public:
		virtual size_t size() = 0;
		virtual size_t offset() = 0;
		virtual bool write(const void *data, size_t len) = 0;
		
		inline bool writeString(const std::string &str) {
			return write(str.c_str(), str.size());
		}
		
		bool writePackedString(int size_type, const std::string &str, bool be = false);
		
		inline bool writeUInt8(uint8_t byte) {
			return write(&byte, 1);
		}
		
		inline bool writeInt8(int8_t byte) {
			return write(&byte, 1);
		}
		
		bool writeUInt16(uint16_t value);
		bool writeUInt16BE(uint16_t value);
		bool writeUInt32(uint32_t value);
		bool writeUInt32BE(uint32_t value);
		bool writeUInt64(uint64_t value);
		bool writeUInt64BE(uint64_t value);
		
		inline bool writeInt16(int16_t value) {
			return writeUInt16(static_cast<uint16_t>(value));
		}
		
		inline bool writeInt16BE(int16_t value) {
			return writeUInt16BE(static_cast<uint16_t>(value));
		}
		
		inline bool writeInt32(int32_t value) {
			return writeUInt32(static_cast<uint32_t>(value));
		}
		
		inline bool writeInt32BE(int32_t value) {
			return writeUInt32BE(static_cast<uint32_t>(value));
		}
		
		inline bool writeInt64(int32_t value) {
			return writeUInt64(static_cast<uint64_t>(value));
		}
		
		inline bool writeInt64BE(int32_t value) {
			return writeUInt64BE(static_cast<uint64_t>(value));
		}
};

class BinaryBufferReader: public BinaryReaderBase {
	protected:
		const uint8_t *m_data = nullptr;
		size_t m_size = 0;
		size_t m_offset = 0;
	public:
		explicit BinaryBufferReader(const std::string &buffer) {
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
		
		bool avail(size_t size) override;
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
		explicit BinaryFileReader(FILE *fp) {
			setFile(fp);
		}
		
		bool setFile(FILE *fp);
		
		bool avail(size_t size) override;
		size_t size() override;
		size_t offset() override;
		bool eof() override;
		bool truncate(size_t len) override;
		bool read(void *data, size_t len) override;
		bool skip(size_t len) override;
};

class BinaryBufferWriter: public BinaryWriterBase {
	protected:
		std::vector<uint8_t> m_buffer;
		size_t m_offset = 0;
	public:
		const uint8_t *buffer() const {
			return &m_buffer[0];
		}
		
		size_t size() override;
		size_t offset() override;
		bool write(const void *data, size_t len) override;
};

class BinaryFileWriter: public BinaryWriterBase {
	protected:
		FILE *m_fp = nullptr;
	public:
		explicit BinaryFileWriter(FILE *fp) {
			setFile(fp);
		}
		
		bool setFile(FILE *fp);
		size_t size() override;
		size_t offset() override;
		bool write(const void *data, size_t len) override;
};
