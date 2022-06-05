#!/bin/sh

. /lib/functions.sh
. /lib/netifd/netifd-proto.sh
init_proto "$@"

proto_usbmodem_init_config() {
	no_device=1
	available=1
	proto_config_add_defaults
}

proto_usbmodem_setup() {
	local interface=$1
	proto_run_command "$interface" usbmodem daemon "$interface"
}

proto_usbmodem_teardown() {
	local interface=$1
	
	proto_init_update "*" 0
	proto_send_update "$interface"
	
	proto_kill_command "$interface"
}

add_protocol usbmodem
