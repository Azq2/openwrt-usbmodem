#pragma once

#include "Log.h"
#include "Ubus.h"
#include "UbusRequest.h"

extern "C" {
#include <libubus.h>
#include <libubox/blobmsg.h>
#include <libubox/list.h>
};

#include <map>
#include <vector>
#include <string>
#include <functional>
#include <memory>

class Ubus;
class UbusRequest;

class UbusObject {
	friend class Ubus;
	
	public:
		typedef std::function<int(std::shared_ptr<UbusRequest>)> Callback;
		
		enum FieldType: int {
			UNSPEC		= BLOBMSG_TYPE_UNSPEC,
			ARRAY		= BLOBMSG_TYPE_ARRAY,
			TABLE		= BLOBMSG_TYPE_TABLE,
			STRING		= BLOBMSG_TYPE_STRING,
			INT64		= BLOBMSG_TYPE_INT64,
			INT32		= BLOBMSG_TYPE_INT32,
			INT16		= BLOBMSG_TYPE_INT16,
			INT8		= BLOBMSG_TYPE_INT8,
			BOOL		= BLOBMSG_TYPE_BOOL,
			DOUBLE		= BLOBMSG_TYPE_DOUBLE,
		};
		
		struct Method {
			struct ubus_method u = {}; // Must be first in struct
			Callback callback;
		};
	protected:
		struct ObjectWrap {
			struct ubus_object o = {}; // Must be first in struct
			struct ubus_object_type t = {};
			UbusObject *self;
		};
		
		bool m_registered = false;
		
		ObjectWrap m_object;
		std::vector<Method> m_methods;
		std::vector<char *> m_names;
		Ubus *m_ubus = nullptr;
		
		char *createName(const char *name, size_t length);
		blobmsg_policy *createBlobmsgPolicy(const std::map<std::string, int> &fields, int *count);
		
		void setParent(Ubus *ubus);
		void setName(const std::string &name);
		
		inline bool isRegistered() const {
			return m_registered;
		}
		
		inline ubus_object *getObject() {
			return &m_object.o;
		}
		
		Method *findMethodByName(const char *name);
		int callHandler(const char *method_name, ubus_request_data *req, blob_attr *attr);
		static int ubusMethodHandler(ubus_context *ctx, ubus_object *obj, ubus_request_data *req, const char *method, blob_attr *attr);
	public:
		void operator=(const UbusObject &) = delete;
		
		UbusObject();
		~UbusObject();
		
		bool attach();
		bool detach();
		
		inline uint32_t id() {
			return m_object.o.id;
		}
		
		UbusObject &method(const std::string &name, const Callback &callback, const std::map<std::string, int> &fields);
};
