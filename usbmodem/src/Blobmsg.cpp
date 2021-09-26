#include <math.h>

#include "Blobmsg.h"
#include "Log.h"

void blobmsgElementToJson(blob_attr *src, json &dst) {
	if (!blobmsg_check_attr(src, false)) {
		dst = nullptr;
		return;
	}
	
	switch (blob_id(src)) {
		case BLOBMSG_TYPE_BOOL:
			dst = *(uint8_t *) blobmsg_data(src) ? true : false;
		break;
		
		case BLOBMSG_TYPE_INT16:
			dst = (int16_t) be16_to_cpu(*(uint16_t *) blobmsg_data(src));
		break;
		
		case BLOBMSG_TYPE_INT32:
			dst = (int32_t) be32_to_cpu(*(uint32_t *) blobmsg_data(src));
		break;
		
		case BLOBMSG_TYPE_INT64:
			dst = (int64_t) be64_to_cpu(*(uint64_t *) blobmsg_data(src));
		break;
		
		case BLOBMSG_TYPE_DOUBLE:
			dst = blobmsg_get_double(src);
		break;
		
		case BLOBMSG_TYPE_STRING:
			dst = std::string((const char *) blobmsg_data(src), blobmsg_data_len(src) - 1);
		return;
		
		case BLOBMSG_TYPE_ARRAY:
			blobmsgToJsonList((blob_attr *) blobmsg_data(src), blobmsg_data_len(src), dst, true);
		return;
		
		case BLOBMSG_TYPE_TABLE:
			blobmsgToJsonList((blob_attr *) blobmsg_data(src), blobmsg_data_len(src), dst, false);
		return;
		
		case BLOBMSG_TYPE_UNSPEC:
		default:
			dst = nullptr;
		break;
	}
}

void blobmsgToJsonList(blob_attr *src, int len, json &dst, bool is_array) {
	if (is_array) {
		dst = json::array();
	} else {
		dst = json::object();
	}
	
	struct blob_attr *pos;
	size_t rem = len;
	
	__blob_for_each_attr(pos, src, rem) {
		if (is_array) {
			blobmsgElementToJson(pos, dst[dst.size()]);
		} else {
			const char *key = blobmsg_name(pos);
			blobmsgElementToJson(pos, dst[key]);
		}
	};
}

void blobmsgToJson(blob_attr *src, json &dst, bool list) {
	bool is_array = blob_is_extended(src) && blobmsg_type(src) == BLOBMSG_TYPE_ARRAY;
	if (list) {
		blobmsgToJsonList((blob_attr *) blobmsg_data(src), blobmsg_data_len(src), dst, is_array);
	} else {
		blobmsgElementToJson(src, dst);
	}
}

bool blobmsgFromJson(blob_buf *dst, const json &src) {
	if (src.type() != json::value_t::object)
		return false;
	
	for (auto &item: src.items())
		blobmsgElementFromJson(dst, item.key().c_str(), item.value());
	
	return true;
}

void blobmsgElementFromJson(blob_buf *dst, const char *key, const json &value) {
	switch (value.type()) {
		case json::value_t::object:
		{
			void *c = blobmsg_open_table(dst, key);
			for (auto &item: value.items())
				blobmsgElementFromJson(dst, item.key().c_str(), item.value());
			blobmsg_close_table(dst, c);
		}
		break;
		
		case json::value_t::array:
		{
			void *c = blobmsg_open_array(dst, key);
			for (auto &item: value)
				blobmsgElementFromJson(dst, nullptr, item);
			blobmsg_close_array(dst, c);
		}
		break;
		
		case json::value_t::boolean:
		{
			blobmsg_add_u8(dst, key, value ? 1 : 0);
		}
		break;
		
		case json::value_t::string:
		{
			const std::string &str = value;
			blobmsg_add_field(dst, BLOBMSG_TYPE_STRING, key, str.c_str(), str.size() + 1);
		}
		break;
		
		case json::value_t::number_integer:
		{
			int64_t i64 = value;
			if (i64 >= std::numeric_limits<int>::min() && i64 <= std::numeric_limits<int>::max()) {
				 blobmsg_add_u32(dst, key, (uint32_t) i64);
			} else {
				 blobmsg_add_u64(dst, key, (uint64_t) i64);
			}
		}
		break;
		
		case json::value_t::number_unsigned:
		{
			uint64_t i64 = value;
			if (i64 <= std::numeric_limits<int>::max()) {
				 blobmsg_add_u32(dst, key, (uint32_t) i64);
			} else {
				 blobmsg_add_u64(dst, key, (uint64_t) i64);
			}
		}
		break;
		
		case json::value_t::number_float:
		{
			double d = value;
			if (isnan(d)) {
				blobmsg_add_field(dst, BLOBMSG_TYPE_UNSPEC, key, NULL, 0);
			} else {
				blobmsg_add_double(dst, key, d);
			}
		}
		break;
		
		default:
			blobmsg_add_field(dst, BLOBMSG_TYPE_UNSPEC, key, NULL, 0);
		break;
	}
}
