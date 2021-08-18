#pragma once

#include "Log.h"
#include "UbusObject.h"

extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
};

#include "Json.h"

#include <map>
#include <vector>
#include <string>
#include <functional>

class UbusObject;

typedef std::function<void(const json &response, int)> UbusResponseCallback;

struct UbusCallRequest {
	blob_buf b = {};
	ubus_request r = {};
	json data;
	UbusResponseCallback callback;
};

struct UbusDeferRequest {
	blob_buf b = {};
	ubus_request_data r = {};
};

class Ubus {
	protected:
		ubus_context *m_ctx = nullptr;
		std::map<std::string, UbusObject> m_objects;
	public:
		bool open();
		void close();
		inline bool avail() const {
			return m_ctx != nullptr;
		}
		
		Ubus();
		~Ubus();
		
		bool call(const std::string &path, const std::string &method, const json &params, const UbusResponseCallback &callback = nullptr, int timeout = 0);
		bool callAsync(const std::string &path, const std::string &method, const json &params, const UbusResponseCallback &callback = nullptr);
		
		UbusObject &object(const std::string &name);
		bool unregisterObject(ubus_object *obj);
		bool registerObject(ubus_object *obj);
		
		UbusDeferRequest *defer(ubus_request_data *original_req);
		bool deferFinish(UbusDeferRequest *req, int status, const json &params, bool cleanup = true);
		bool reply(ubus_request_data *req, const json &params);
		
		static void onCallComplete(ubus_request *r, int ret);
		static void onCallData(ubus_request *r, int type, blob_attr *msg);
};
