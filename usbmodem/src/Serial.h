#pragma once

#include <string>
#include <termios.h>

#include "Utils.h"

class Serial {
	protected:
		int m_fd = -1;
		bool m_ignore_eintr = false;
	
	public:
		enum Errors {
			ERR_SUCCESS		= 0,
			ERR_IO			= -1,
			ERR_BROKEN		= -2
		};
		
		Serial();
		~Serial();
		
		static speed_t getBaudrate(int speed);
		
		int open(std::string device, int speed);
		int close();
		
		int read(char *data, int size, int timeout_ms = 10000);
		int write(const char *data, int size, int timeout_ms = 10000);
		
		int readChunk(char *data, int size, int timeout_ms = 10000);
		int writeChunk(const char *data, int size, int timeout_ms = 10000);
		
		inline void setIgnoreInterrupts(bool flag) {
			m_ignore_eintr = flag;
		}
};
