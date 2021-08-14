#pragma once

#include "Log.h"

extern "C" {
#include <libubus.h>
#include <libubox/blobmsg_json.h>
};

#include <json-c/json.h>

#include <string>
#include <functional>

class Ubus {
	protected:
		struct UBusRequest {
			blob_buf b = {};
			ubus_request r = {};
			json_object *data = nullptr;
			std::function<void(json_object *, int)> callback;
		};
		
		ubus_context *m_ubus = nullptr;
	
	public:
		bool open();
		void close();
		inline bool avail() {
			return m_ubus != nullptr;
		}
		
		Ubus();
		~Ubus();
		
		bool call(const std::string &path, const std::string &method, json_object *params, const std::function<void(json_object *, int)> &callback = nullptr, int timeout = 0);
		bool callAsync(const std::string &path, const std::string &method, json_object *params, const std::function<void(json_object *, int)> &callback = nullptr);
		
		static void onCallComplete(ubus_request *r, int ret);
		static void onCallData(ubus_request *r, int type, blob_attr *msg);
};
