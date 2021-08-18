#pragma once

#include "Log.h"
#include "Modem.h"
#include "Loop.h"
#include "Ubus.h"

#include <signal.h>

#include <map>
#include <string>

class ModemServiceApi {
	protected:
	public:
		explicit ModemServiceApi();
		void run();
};
