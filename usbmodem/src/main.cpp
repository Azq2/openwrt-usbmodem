#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>

#include <Core/Log.h>
#include <Core/Loop.h>
#include <Core/UbusLoop.h>
#include <Core/Utils.h>
#include <Core/GsmUtils.h>
#include <Core/AtParser.h>
#include <Core/AtChannel.h>
#include <Core/Serial.h>
#include <Core/SmsDb.h>

#include "Modem.h"
#include "Modem/BaseAt.h"
#include "Modem/Asr1802.h"
#include "ModemService.h"
#include "UsbDiscover.h"

static int modemDaemon(int argc, char *argv[]) {
	if (argc == 3) {
		ModemService s(argv[2]);
		return s.run();
	}
	fprintf(stderr, "usage: %s daemon <iface>\n", argv[0]);
	return -1;
}

struct RawPdu {
	SmsDb::SmsType type;
	int id;
	std::string hex;
};

static int test(int argc, char *argv[]) {
	LOGD("test???\n");
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 1) {
		if (strcmp(argv[1], "discover") == 0)
			return UsbDiscover::run(argc - 2, argv + 2);
		
		if (strcmp(argv[1], "daemon") == 0)
			return modemDaemon(argc - 2, argv + 2);
		
		if (strcmp(argv[1], "test") == 0)
			return test(argc - 2, argv + 2);
	}
	
	fprintf(stderr, "usage: usbmodem <action>\n");
	fprintf(stderr, "  usbmodem daemon <iface>    - start modem daemon\n");
	
	return -1;
}
