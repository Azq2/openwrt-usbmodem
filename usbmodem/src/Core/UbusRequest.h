#pragma once

#include "Log.h"
#include "Json.h"
#include "Blobmsg.h"
#include "Ubus.h"
#include "Utils.h"

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
		json m_json;
		bool m_json_ready = false;
		bool m_done = false;
		size_t m_req_id = 0;
		int64_t m_time = 0;
		static size_t m_global_req_id;
	public:
		UbusRequest(Ubus *ubus, ubus_request_data *req, blob_attr *data);
		~UbusRequest();
		
		inline size_t id() const {
			return m_req_id;
		}
		
		inline std::string uniqKey() const {
			return strprintf("%lld_%u", m_time, m_req_id);
		}
		
		inline bool done() const {
			return m_done;
		}
		
		bool reply(const json &params, int status = 0);
		void defer();
		const json &data();
};
