#pragma once

#include <string>
#include <termios.h>

#include "Utils.h"

class Serial {
	protected:
		int m_fd = -1;
	
	public:
		enum Errors {
			ERR_SUCCESS		= 0,
			ERR_IO			= -1,
			ERR_BROKEN		= -2,
			ERR_INTR		= -3
		};
		
		Serial();
		~Serial();
		
		int m_wake_fds[2] = {-1, -1};
		
		static speed_t getBaudrate(int speed);
		
		int open(const std::string &device, int speed);
		int close();
		void breakTransfer();
		
		int read(char *data, int size, int timeout_ms = 10000);
		int write(const char *data, int size, int timeout_ms = 10000);
		
		int readChunk(char *data, int size, int timeout_ms = 10000);
		int writeChunk(const char *data, int size, int timeout_ms = 10000);
};
