#!/bin/bash
# Dev script for test on real router without rebuilding openwrt
# Example usage:
# TEST_ROUTER=root@192.168.1.1 TOPDIR=~/build/openwrt ./test.sh

VERSION=7
DIR=$(readlink -f $0)
DIR=$(dirname $0)
MODE=$1

if [[ $TOPDIR == "" ]]; then
	# Openwrt build root
	export TOPDIR=~/openwrt
fi

if [[ $TEST_ROUTER == "" ]]; then
	# Test router
	TEST_ROUTER=root@192.168.2.1
fi

export PATH="$PATH:$TOPDIR/staging_dir/host/bin"

echo "MODE=$MODE"

# Build package
echo "Build..."
make -C "$DIR/usbmodem" -j9 compile || exit 1

function install_file {
	src="$1"
	dst="$2"
	tmp_dst=/tmp/usbmodem.$(echo $dst | md5sum | awk '{print $1}')
	
	if [[ $MODE == "install" ]]; then
		ssh $TEST_ROUTER rm -rf "$tmp_dst"
		ssh $TEST_ROUTER rm -rf "$dst"
		scp -r "$src" "$TEST_ROUTER:$dst"
	else
		ssh $TEST_ROUTER rm -rf "$tmp_dst"
		scp -r "$src" "$TEST_ROUTER:$tmp_dst"
		
		if [[ $NEED_SETUP != "0" ]];
		then
			ssh $TEST_ROUTER rm -rf "$dst"
			ssh $TEST_ROUTER ln -s "$tmp_dst" "$dst"
		fi
	fi
}

NEED_SETUP=$(ssh $TEST_ROUTER ls /tmp/usbmodem_ok.$VERSION 2>&1 > /dev/null; echo $?)

echo "Send..."
install_file $DIR/luci-app-usbmodem/htdocs/luci-static/resources/usbmodem.js				/www/luci-static/resources/usbmodem.js
install_file $DIR/luci-app-usbmodem/htdocs/luci-static/resources/view/usbmodem				/www/luci-static/resources/view/usbmodem
install_file $DIR/luci-proto-usbmodem/htdocs/luci-static/resources/protocol/usbmodem.js		/www/luci-static/resources/protocol/usbmodem.js

install_file $DIR/luci-proto-usbmodem/root/usr/share/rpcd/acl.d/luci-proto-usbmodem.json	/usr/share/rpcd/acl.d/luci-proto-usbmodem.json
install_file $DIR/luci-app-usbmodem/root/usr/share/rpcd/acl.d/luci-app-usbmodem.json		/usr/share/rpcd/acl.d/luci-app-usbmodem.json
install_file $DIR/luci-app-usbmodem/root/usr/share/luci/menu.d/luci-app-usbmodem.json		/usr/share/luci/menu.d/luci-app-usbmodem.json

install_file $DIR/usbmodem/files/usbmodem.sh												/lib/netifd/proto/usbmodem.sh
install_file $DIR/usbmodem/files/usbmodem.user												/etc/usbmodem.sh
install_file $DIR/usbmodem/files/usbmodem.usb												/etc/hotplug.d/tty/30-usbmodem

ssh $TEST_ROUTER killall -9 usbmodem
install_file $TOPDIR/build_dir/*/usbmodem/ipkg-install/usr/sbin/usbmodem					/usr/sbin/usbmodem

if [[ $NEED_SETUP != "0" ]];
then
	echo "Need setup..."
	ssh $TEST_ROUTER /etc/init.d/rpcd restart
	ssh $TEST_ROUTER /etc/init.d/network restart
	ssh $TEST_ROUTER touch /tmp/usbmodem_ok.$VERSION
	exit 0
fi
