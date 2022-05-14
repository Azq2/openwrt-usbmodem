#include "UbusRequest.h"

UbusRequest::UbusRequest(Ubus *ubus, ubus_request_data *req, blob_attr *data): m_ubus(ubus), m_req(req), m_data(data) {
	
}

UbusRequest::~UbusRequest() {
	// Make sure all request are finished
	if (m_defer_req) {
		LOGE("Ubus defer request not finised!!!\n");
		reply(nullptr, UBUS_STATUS_UNKNOWN_ERROR);
	}
}

const json &UbusRequest::data() {
	if (!m_json_ready) {
		m_json_ready = true;
		blobmsgToJson(m_data, m_json);
	}
	return m_json;
}

bool UbusRequest::reply(const json &params, int status) {
	if (m_done) {
		LOGE("Request already replied!\n");
		return false;
	}
	
	if (status != 0 && !m_defer_req) {
		LOGE("Status not allowed when request is not defer!\n");
		return false;
	}
	
	if (m_defer_req) {
		if (m_ubus->deferFinish(m_defer_req, status, params, false)) {
			m_done = true;
			m_defer_req = nullptr;
			return true;
		}
	} else {
		if (m_ubus->reply(m_req, params)) {
			m_done = true;
			return true;
		}
	}
	
	return false;
}

void UbusRequest::defer() {
	if (!m_defer_req && !m_done)
		m_defer_req = m_ubus->defer(m_req);
}
