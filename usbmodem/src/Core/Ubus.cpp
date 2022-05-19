#include "Ubus.h"
#include "Utils.h"
#include "UbusLoop.h"

extern "C" {
#include <libubox/blobmsg_json.h>
};

Ubus::Ubus() {
	
}

Ubus::~Ubus() {
	close();
}

void Ubus::close() {
	UbusLoop::assertThread();
	
	if (m_ctx) {
		ubus_free(m_ctx);
		m_ctx = nullptr;
	}
}

bool Ubus::open() {
	UbusLoop::assertThread();
	
	m_ctx = ubus_connect(nullptr);
	if (!m_ctx)
		return false;
	
	ubus_add_uloop(m_ctx);
	
	return true;
}

void Ubus::onCallData(ubus_request *r, int type, blob_attr *msg) {
	UbusCallRequest *req = static_cast<UbusCallRequest *>(r->priv);
	blobmsgToJson(msg, req->data);
}

void Ubus::onCallComplete(ubus_request *r, int ret) {
	UbusCallRequest *req = static_cast<UbusCallRequest *>(r->priv);
	if (req->callback)
		req->callback(req->data, ret);
	
	blob_buf_free(&req->b);
	req->data.clear();
	
	delete req;
}

UbusObject &Ubus::object(const std::string &name) {
	auto it = m_objects.find(name);
	if (it == m_objects.end()) {
		UbusObject &obj = m_objects[name];
		obj.setName(name);
		obj.setParent(this);
		return obj;
	}
	return it->second;
}

bool Ubus::registerObject(ubus_object *obj) {
	UbusLoop::assertThread();
	return ubus_add_object(m_ctx, obj) == 0;
}

bool Ubus::unregisterObject(ubus_object *obj) {
	UbusLoop::assertThread();
	return ubus_remove_object(m_ctx, obj) == 0;
}

bool Ubus::callAsync(const std::string &path, const std::string &method, const json &params, const UbusResponseCallback &callback) {
	if (!UbusLoop::isOwnThread()) {
		return UbusLoop::exec<bool>([=, this]() {
			return callAsync(path, method, params, callback);
		});
	}
	
	uint32_t id;
	if (ubus_lookup_id(m_ctx, path.c_str(), &id) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	UbusCallRequest *req = new UbusCallRequest;
	
	// Init msg
	blob_buf_init(&req->b, 0);
	blobmsgFromJson(&req->b, params);
	
	// Init request
	if (ubus_invoke_async(m_ctx, id, method.c_str(), req->b.head, &req->r) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	// Setup callbacks
	req->r.priv = static_cast<void *>(req);
	req->r.data_cb = onCallData;
	req->r.complete_cb = onCallComplete;
	req->callback = callback;
	
	// Start request
	ubus_complete_request_async(m_ctx, &req->r);
	
	return true;
}

bool Ubus::call(const std::string &path, const std::string &method, const json &params, const UbusResponseCallback &callback, int timeout) {
	if (!UbusLoop::isOwnThread()) {
		return UbusLoop::exec<bool>([=, this]() {
			return call(path, method, params, callback, timeout);
		});
	}
	
	uint32_t id;
	if (ubus_lookup_id(m_ctx, path.c_str(), &id) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	UbusCallRequest *req = new UbusCallRequest;
	
	// Init msg
	blob_buf_init(&req->b, 0);
	blobmsgFromJson(&req->b, params);
	
	// Init request
	if (ubus_invoke_async_fd(m_ctx, id, method.c_str(), req->b.head, &req->r, -1) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	// Setup callbacks
	req->r.priv = static_cast<void *>(req);
	req->r.data_cb = onCallData;
	req->r.complete_cb = onCallComplete;
	req->callback = callback;
	
	// Start request
	return ubus_complete_request(m_ctx, &req->r, timeout) == 0;
}

bool Ubus::reply(ubus_request_data *req, const json &params) {
	UbusLoop::assertThread();
	
	blob_buf b = {};
	blob_buf_init(&b, 0);
	blobmsgFromJson(&b, params);
	
	int ret = ubus_send_reply(m_ctx, req, b.head);
	blob_buf_free(&b);
	
	return ret == 0;
}

bool Ubus::deferFinish(UbusDeferRequest *req, int status, const json &params, bool cleanup) {
	UbusLoop::assertThread();
	
	if (status == UBUS_STATUS_OK) {
		blobmsgFromJson(&req->b, params);
		ubus_send_reply(m_ctx, &req->r, req->b.head);
	}
	ubus_complete_deferred_request(m_ctx, &req->r, status);
	blob_buf_free(&req->b);
	if (cleanup)
		delete req;
	return true;
}

UbusDeferRequest *Ubus::defer(ubus_request_data *original_req) {
	UbusLoop::assertThread();
	
	UbusDeferRequest *req = new UbusDeferRequest;
	blob_buf_init(&req->b, 0);
	ubus_defer_request(m_ctx, original_req, &req->r);
	return req;
}
