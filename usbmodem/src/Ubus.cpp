#include "Ubus.h"

Ubus::Ubus() {
	
}

Ubus::~Ubus() {
	close();
}

void Ubus::close() {
	if (m_ubus) {
		ubus_free(m_ubus);
		m_ubus = nullptr;
	}
}

bool Ubus::open() {
	m_ubus = ubus_connect(nullptr);
	if (!m_ubus)
		return false;
	
	ubus_add_uloop(m_ubus);
	
	return true;
}

void Ubus::onCallData(ubus_request *r, int type, blob_attr *msg) {
	UBusRequest *req = static_cast<UBusRequest *>(r->priv);
	
	char *json_str = blobmsg_format_json(msg, true);
	req->data = json_str ? json_tokener_parse(json_str) : nullptr;
	
	if (json_str)
		free(json_str);
}

void Ubus::onCallComplete(ubus_request *r, int ret) {
	UBusRequest *req = static_cast<UBusRequest *>(r->priv);
	if (req->callback)
		req->callback(req->data, ret);
	
	blob_buf_free(&req->b);
	
	if (req->data)
		json_object_put(req->data);
	delete req;
}

bool Ubus::callAsync(const std::string &path, const std::string &method, json_object *params, const std::function<void(json_object *, int)> &callback) {
	uint32_t id;
	if (ubus_lookup_id(m_ubus, path.c_str(), &id) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	UBusRequest *req = new UBusRequest;
	
	// Init msg
	blob_buf_init(&req->b, 0);
	
	if (params)
		blobmsg_add_object(&req->b, params);
	
	// Init request
	if (ubus_invoke_async(m_ubus, id, method.c_str(), req->b.head, &req->r) != 0) {
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
	ubus_complete_request_async(m_ubus, &req->r);
	
	return true;
}

bool Ubus::call(const std::string &path, const std::string &method, json_object *params, const std::function<void(json_object *, int)> &callback, int timeout) {
	uint32_t id;
	if (ubus_lookup_id(m_ubus, path.c_str(), &id) != 0) {
		if (callback)
			callback(nullptr, -1);
		return false;
	}
	
	UBusRequest *req = new UBusRequest;
	
	// Init msg
	blob_buf_init(&req->b, 0);
	
	if (params)
		blobmsg_add_object(&req->b, params);
	
	// Init request
	if (ubus_invoke_async_fd(m_ubus, id, method.c_str(), req->b.head, &req->r, -1) != 0) {
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
	return ubus_complete_request(m_ubus, &req->r, timeout) == 0;
}
