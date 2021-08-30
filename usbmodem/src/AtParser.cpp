#include "AtParser.h"

bool AtParser::parseNextString(std::string *value) {
	const char *start, *end;
	m_cursor = parseNextArg(m_cursor, &start, &end);
	arg_cnt++;
	
	if (m_cursor) {
		value->assign(start, end - start);
		return true;
	}
	
	LOGE("AtParser:%s: can't parse #%d argument in '%s'\n", __FUNCTION__, arg_cnt, m_str);
	
	m_success = false;
	return false;
}

bool AtParser::parseNextInt(int32_t *value, int base) {
	const char *start, *end;
	m_cursor = parseNextArg(m_cursor, &start, &end);
	arg_cnt++;
	
	if (parseNumeric(start, end, base, false, value))
		return true;
	
	LOGE("AtParser:%s: can't parse #%d argument in '%s'\n", __FUNCTION__, arg_cnt, m_str);
	
	m_success = false;
	return false;
}

bool AtParser::parseNextUInt(uint32_t *value, int base) {
	const char *start, *end;
	m_cursor = parseNextArg(m_cursor, &start, &end);
	arg_cnt++;
	
	if (parseNumeric(start, end, base, true, value))
		return true;
	
	LOGE("AtParser:%s: can't parse #%d argument in '%s'\n", __FUNCTION__, arg_cnt, m_str);
	
	m_success = false;
	return false;
}

bool AtParser::parseNextBool(bool *value) {
	int32_t v;
	if (parseNextInt(&v)) {
		*value = v == 1;
		if (v == 1 || v == 0) {
			return true;
		} else {
			m_success = false;
			LOGE("AtParser:%s: can't parse #%d argument in '%s'\n", __FUNCTION__, arg_cnt, m_str);
		}
	}
	return false;
}

bool AtParser::parseNumeric(const char *start, const char *end, int base, bool is_unsigned, void *out) {
	char *number_end = nullptr;
	
	if (!start)
		return false;
	
	if (is_unsigned) {
		uint32_t *ptr = static_cast<uint32_t*>(out);
		*ptr = strtoul(start, &number_end, base);
	} else {
		int32_t *ptr = static_cast<int32_t*>(out);
		*ptr = strtoul(start, &number_end, base);
	}
	return number_end != start && number_end <= end;
}

bool AtParser::parseNextNewLine() {
	arg_cnt++;
	
	while (isspace(*m_cursor) && *m_cursor != '\n')
		m_cursor++;
	
	if (*m_cursor != '\n') {
		LOGE("AtParser:%s: can't parse #%d argument (new line) in '%s'\n", __FUNCTION__, arg_cnt, m_str);
		m_success = false;
		return false;
	}
	
	m_cursor++;
	
	return true;
}

bool AtParser::parseNextSkip() {
	const char *start, *end;
	m_cursor = parseNextArg(m_cursor, &start, &end);
	arg_cnt++;
	
	if (m_cursor)
		return true;
	
	LOGE("AtParser:%s: can't parse #%d argument in '%s'\n", __FUNCTION__, arg_cnt, m_str);
	
	m_success = false;
	return false;
}

int AtParser::getArgCnt(const std::string &value) {
	const char *start, *end, *cursor = value.c_str();
	int count = 0;
	do {
		cursor = parseNextArg(cursor, &start, &end);
		if (cursor)
			count++;
	} while (cursor && *cursor);
	return count;
}

const char *AtParser::parseNextArg(const char *str, const char **start, const char **end) {
	const char *cursor = str;
	
	if (!cursor)
		return nullptr;
	
	*start = *end = nullptr;
	
	// Skip spaces
	while (isspace(*cursor) && *cursor != '\n')
		cursor++;
	
	// Is quoted value
	if (*cursor == '"') {
		cursor++;
		
		*start = cursor;
		
		// Wait for next "
		while (*cursor && *cursor != '"')
			cursor++;
		
		*end = cursor;
		
		if (*cursor != '"')
			return nullptr;
		
		cursor++;
		
		// Skip spaces
		while (isspace(*cursor) && *cursor != '\n')
			cursor++;
		
		// Success, if next char is arg separator or string ended
		if (!*cursor || *cursor == ',' || *cursor == '\n')
			return *cursor == ',' ? cursor + 1 : cursor;
	}
	// Is raw value
	else {
		*start = cursor;
		
		// Wait for next argument or EOF
		while (*cursor && (*cursor != ',' && *cursor != '\n'))
			cursor++;
		
		*end = cursor;
		
		return *cursor == ',' ? cursor + 1 : cursor;
	}
	
	return nullptr;
}
