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

static int test(int argc, char *argv[]) {
	LOGD("test??? %s\n", urldecode("xuj%3F%3F%3Fpizda+jgurda").c_str());
	auto found = UsbDiscover::findTTY("usb://201e:10f8/tty1?name=Haier+CE81b&serial=");
	LOGD("found=%s\n", found.c_str());
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc > 1) {
		if (strcmp(argv[1], "discover") == 0)
			return UsbDiscover::run(argc - 2, argv + 2);
		
		if (strcmp(argv[1], "daemon") == 0 || strcmp(argv[1], "check") == 0)
			return ModemService::run(argv[1], argc - 2, argv + 2);
		
		if (strcmp(argv[1], "test") == 0)
			return test(argc - 2, argv + 2);
	}
	
	fprintf(stderr, "usage: usbmodem <action>\n");
	fprintf(stderr, "  usbmodem daemon <iface>    - start modem daemon\n");
	fprintf(stderr, "  usbmodem discover          - show available modems\n");
	fprintf(stderr, "  usbmodem check             - recheck available interfaces\n");
	
	return -1;
}
