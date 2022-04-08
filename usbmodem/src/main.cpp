#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include "ModemService.h"
#include "UsbDiscover.h"
#include "Log.h"
#include "Loop.h"
#include "Utils.h"
#include "GsmUtils.h"
#include "AtParser.h"

static int modemDaemon(int argc, char *argv[]) {
	if (argc == 3) {
		ModemService s(argv[2]);
		return s.run();
	}
	
	fprintf(stderr, "usage: %s daemon <iface>\n", argv[0]);
	return -1;
}

static int test(int argc, char *argv[]) {
	// std::string line = "+CPMS: (\"SM\"),(\"ME\"),(\"SM\")";
	std::string line = "+CPMS: (\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\"),(\"ME\",\"MT\",\"SM\",\"SR\")";
	
	std::vector<std::string> mem1;
	std::vector<std::string> mem2;
	std::vector<std::string> mem3;
	
	int arg_cnt = AtParser::getArgCnt(line);
	
	bool success = AtParser(line)
		.parseArray(&mem1)
		.parseArray(&mem2)
		.parseArray(&mem3)
		.success();
	
	LOGD("arg_cnt=%d\n", arg_cnt);
	
	LOGD("mem1:");
	for (auto &m: mem1)
		LOGD(" %s", m.c_str());
	LOGD("\n");
	
	LOGD("mem2:");
	for (auto &m: mem2)
		LOGD(" %s", m.c_str());
	LOGD("\n");
	
	LOGD("mem3:");
	for (auto &m: mem3)
		LOGD(" %s", m.c_str());
	LOGD("\n");
	
	LOGD("success = %d\n", success);
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
