#!/bin/sh

. /lib/functions.sh
. /lib/netifd/netifd-proto.sh
init_proto "$@"

proto_usbmodem_init_config() {
	no_device=1
	available=1
	
	proto_config_add_string modem_device
	proto_config_add_string modem_type
	proto_config_add_string pdp_type
	proto_config_add_string apn
	proto_config_add_string pincode
	proto_config_add_string auth_type
	proto_config_add_string username
	proto_config_add_string password
	proto_config_add_string force_use_dhcp
	proto_config_add_string force_network_restart
	proto_config_add_string force_network_restart
	proto_config_add_defaults
}

proto_usbmodem_setup() {
	local interface=$1
	
	local modem_device
	json_get_vars modem_device
	
	# Check device
	avail=$(usbmodem check "$modem_device")
	[ $avail != "1" ] && {
		echo "Device not found: $modem_device"
		proto_set_available $interface 0
		return 1
	}
	
	# Start modem service daemon
	proto_run_command "$interface" usbmodem daemon "$interface"
}

proto_usbmodem_teardown() {
	local interface=$1
	
	proto_init_update "*" 0
	proto_send_update "$interface"
	
	proto_kill_command "$interface"
}

add_protocol usbmodem
