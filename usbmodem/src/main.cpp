#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>

#include <Core/UsbDiscover.h>
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
	std::vector<RawPdu> sms_list = {

	};
	
	SmsDb db;
	for (auto r: sms_list)
		db.loadRawPdu(r.type, r.id, r.hex);
	db.save();
	
	LOGD("test???\n");
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 1) {
		if (strcmp(argv[1], "discover") == 0)
			return discoverUsbModems(argc, argv);
		if (strcmp(argv[1], "check") == 0)
			return checkDevice(argc, argv);
		if (strcmp(argv[1], "ifname") == 0)
			return findIfname(argc, argv);
		if (strcmp(argv[1], "daemon") == 0)
			return modemDaemon(argc, argv);
		if (strcmp(argv[1], "test") == 0)
			return test(argc, argv);
		
	}
	
	fprintf(stderr, "usage: %s <action>\n", argv[0]);
	fprintf(stderr, "  %s discover - discover usb modems\n", argv[0]);
	fprintf(stderr, "  %s check <device> - check if device available\n", argv[0]);
	fprintf(stderr, "  %s ifname <device> - get network device by tty\n", argv[0]);
	fprintf(stderr, "  %s daemon <iface> - start modem daemon\n", argv[0]);
	
	return -1;
}
