#pragma once

#include <string>
#include <vector>

#include "Log.h"

class AtParser {
	protected:
		const char *m_str = nullptr;
		const char *m_cursor = nullptr;
		int arg_cnt = 0;
		bool m_success = false;
		
		static const char *parseNextArg(const char *str, const char **start, const char **end);
		static bool parseNumeric(const char *start, const char *end, int base, bool is_unsigned, void *out);
		
		static const char *skipSpaces(const char *cursor);
	public:
		explicit AtParser(const std::string &s) {
			parse(s.c_str());
		}
		
		explicit AtParser(const char *s) {
			parse(s);
		}
		
		AtParser() { }
		
		static int getArgCnt(const std::string &value);
		
		inline AtParser &reset() {
			m_cursor = m_str;
			m_success = true;
			arg_cnt = 0;
			return *this;
		}
		
		inline AtParser &parse(const char *s) {
			m_str = s;
			m_cursor = s;
			
			// Skip prefix
			while (*m_cursor && *m_cursor != ':')
				m_cursor++;
			
			if (*m_cursor) {
				m_cursor++;
			} else {
				m_cursor = s;
			}
			
			m_success = true;
			arg_cnt = 0;
			
			return *this;
		}
		
		constexpr bool success() const {
			return m_success;
		}
		
		inline AtParser &parse(const std::string &s) {
			return parse(s.c_str());
		}
		
		inline AtParser &parseString(std::string *value) {
			parseNextString(value);
			return *this;
		}
		
		inline AtParser &parseInt(int32_t *value, int base = 10) {
			parseNextInt(value, base);
			return *this;
		}
		
		inline AtParser &parseUInt(uint32_t *value, int base = 10) {
			parseNextUInt(value, base);
			return *this;
		}
		
		inline AtParser &parseBool(bool *value) {
			parseNextBool(value);
			return *this;
		}
		
		inline AtParser &parseArray(std::vector<std::string> *values) {
			parseNextArray(values);
			return *this;
		}
		
		inline AtParser &parseList(std::vector<std::string> *values) {
			parseNextList(values);
			return *this;
		}
		
		inline AtParser &parseNewLine() {
			parseNextNewLine();
			return *this;
		}
		
		inline AtParser &parseSkip() {
			parseNextSkip();
			return *this;
		}
		
		bool parseNextString(std::string *value);
		bool parseNextInt(int32_t *value, int base = 10);
		bool parseNextUInt(uint32_t *value, int base = 10);
		bool parseNextBool(bool *value);
		bool parseNextArray(std::vector<std::string> *values);
		bool parseNextList(std::vector<std::string> *values);
		bool parseNextNewLine();
		bool parseNextSkip();
};
