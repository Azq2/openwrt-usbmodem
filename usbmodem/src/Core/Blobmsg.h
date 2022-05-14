#pragma once

#include "Json.h"

extern "C" {
#include <libubox/blobmsg.h>
};

void blobmsgToJson(blob_attr *src, json &dst, bool list = true);
inline void blobmsgToJson(blob_buf *src, json &dst, bool list = true) {
	blobmsgToJson(src->head, dst, list);
}

bool blobmsgFromJson(blob_buf *dst, const json &src);

void blobmsgElementToJson(blob_attr *src, json &dst);
void blobmsgToJsonList(blob_attr *src, int len, json &dst, bool is_array);
void blobmsgElementFromJson(blob_buf *dst, const char *key, const json &value);
