#pragma once

#include "Log.h"
#include "Json.h"
#include "Ubus.h"

extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/list.h>
};

#include <map>
#include <vector>
#include <string>
#include <functional>

class Ubus;
class UbusDeferRequest;

class UbusRequest {
	protected:
		Ubus *m_ubus = nullptr;
		UbusDeferRequest *m_defer_req = nullptr;
		ubus_request_data *m_req = nullptr;
		blob_attr *m_data = nullptr;
		bool m_done = false;
	public:
		UbusRequest(Ubus *ubus, ubus_request_data *req, blob_attr *data);
		~UbusRequest();
		bool reply(const json &params, int status = 0);
		void defer();
};
