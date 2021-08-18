#include "UbusObject.h"

UbusObject::UbusObject() {
	m_object.self = this;
	m_object.t.id = 0;
	m_object.t.name = nullptr;
	m_object.t.n_methods = 0;
	m_object.t.methods = nullptr;
	
	m_object.o.type = &m_object.t;
	m_object.o.id = 0;
	m_object.o.name = nullptr;
	m_object.o.n_methods = 0;
	m_object.o.methods = nullptr;
}

void UbusObject::setName(const std::string &name) {
	m_object.o.name = createName(name.c_str(), name.size());
	m_object.t.name = m_object.o.name;
}

void UbusObject::setParent(Ubus *ubus) {
	m_ubus = ubus;
}

char *UbusObject::createName(const char *name, size_t length) {
	char *ptr = strndup(name, length);
	m_names.push_back(ptr);
	return ptr;
}

blobmsg_policy *UbusObject::createBlobmsgPolicy(const std::map<std::string, int> &fields, int *count) {
	if (!fields.size()) {
		*count = 0;
		return nullptr;
	}
	
	int n_policy = 0;
	blobmsg_policy *policy = new blobmsg_policy[fields.size()];
	
	for (const auto &field: fields) {
		policy[n_policy].name = createName(field.first.c_str(), field.first.size());
		policy[n_policy].type = static_cast<enum blobmsg_type>(field.second);
		n_policy++;
	}
	
	*count = n_policy;
	return policy;
}

UbusObject::Method *UbusObject::findMethodByName(const char *name) {
	for (auto &m: m_methods) {
		if (strcmp(m.u.name, name) == 0)
			return &m;
	}
	return nullptr;
}

int UbusObject::callHandler(const char *method_name, ubus_request_data *req, blob_attr *msg) {
	Method *method = findMethodByName(method_name);
	if (!method)
		return UBUS_STATUS_METHOD_NOT_FOUND;
	return method->callback(std::make_shared<UbusRequest>(m_ubus, req, msg));
}

int UbusObject::ubusMethodHandler(ubus_context *ctx, ubus_object *obj, ubus_request_data *req, const char *method, blob_attr *attr) {
	ObjectWrap *object_wrap = reinterpret_cast<ObjectWrap *>(obj);
	return object_wrap->self->callHandler(method, req, attr);
}

UbusObject &UbusObject::method(const std::string &name, const Callback &callback, const std::map<std::string, int> &fields) {
	if (m_registered)
		throw std::runtime_error("UbusObject is readonly, because it already registered to ubus.");
	
	m_methods.resize(m_methods.size() + 1);
	m_object.o.methods = reinterpret_cast<ubus_method *>(m_methods.data());
	m_object.o.n_methods = m_methods.size();
	
	m_object.t.methods = m_object.o.methods;
	m_object.t.n_methods = m_object.o.n_methods;
	
	Method &method = m_methods.back();
	method.u.name = createName(name.c_str(), name.size());
	method.u.handler = ubusMethodHandler;
	method.u.policy = createBlobmsgPolicy(fields, &method.u.n_policy);
	method.callback = callback;
	
	return *this;
}

bool UbusObject::attach() {
	if (m_registered)
		return true;
	
	if (m_ubus->registerObject(this->getObject())) {
		m_registered = true;
		return true;
	}
	return false;
}

bool UbusObject::detach() {
	if (!m_registered)
		return false;
	
	if (m_ubus->unregisterObject(this->getObject())) {
		m_registered = false;
		return true;
	}
	return false;
}

UbusObject::~UbusObject() {
	m_object.o.name = nullptr;
	m_object.o.methods = nullptr;
	m_object.o.n_methods = 0;
	
	for (auto &p: m_names)
		free(p);
	m_names.clear();
	
	for (auto &m: m_methods) {
		if (m.u.policy)
			delete[] m.u.policy;
	}
	m_methods.clear();
}
