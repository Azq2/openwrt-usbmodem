#!/bin/bash
# Dev script for test on real router without rebuilding openwrt
# Example usage:
# TEST_ROUTER=root@192.168.1.1 TOPDIR=~/build/openwrt ./test.sh

DIR=$(readlink -f $0)
DIR=$(dirname $0)

if [[ $TOPDIR == "" ]]; then
	# Openwrt build root
	export TOPDIR=/openwrt
fi

if [[ $TEST_ROUTER == "" ]]; then
	# Test router
	TEST_ROUTER=root@192.168.1.155
fi

export PATH="$PATH:$TOPDIR/staging_dir/host/bin"

# Build package
echo "Build..."
make -C "$DIR/usbmodem" compile || exit 1

function install_links {
	src="$1"
	dst="$2"
	
	ssh $TEST_ROUTER rm "$dst"
	ssh $TEST_ROUTER ln -s "/tmp/$src" "$dst"
}

function copy_files {
	ssh $TEST_ROUTER rm -rf /tmp/usbmodem-view
	scp -r "$DIR/luci-app-usbmodem/htdocs/luci-static/resources/view/usbmodem" $TEST_ROUTER:/tmp/usbmodem-view
	scp "$DIR/luci-proto-usbmodem/htdocs/luci-static/resources/protocol/usbmodem.js" $TEST_ROUTER:/tmp
	scp "$DIR/luci-proto-usbmodem/root/usr/share/rpcd/acl.d/luci-proto-usbmodem.json" $TEST_ROUTER:/tmp/acl-proto-usbmodem.json
	scp "$DIR/luci-app-usbmodem/root/usr/share/rpcd/acl.d/luci-app-usbmodem.json" $TEST_ROUTER:/tmp/acl-app-usbmodem.json
	scp "$DIR/luci-app-usbmodem/root/usr/share/luci/menu.d/luci-app-usbmodem.json" $TEST_ROUTER:/tmp/menu-app-usbmodem.json
	scp "$DIR/usbmodem/files/usbmodem.usb" $TEST_ROUTER:/tmp
	scp "$DIR/usbmodem/files/usbmodem.sh" $TEST_ROUTER:/tmp
	scp "$DIR/usbmodem/files/usbmodem.user" $TEST_ROUTER:/tmp
	# ssh $TEST_ROUTER killall -9 usbmodem
	scp $TOPDIR/build_dir/*/usbmodem/ipkg-install/usr/sbin/usbmodem $TEST_ROUTER:/tmp
}

ret=$(ssh $TEST_ROUTER ls /tmp/usbmodem_ok 2>&1 > /dev/null; echo $?)

if [[ $ret != "0" ]];
then
	echo "need setup..."
	install_links usbmodem-view /www/luci-static/resources/view/usbmodem
	install_links usbmodem.sh /lib/netifd/proto/usbmodem.sh
	install_links usbmodem.user /etc/usbmodem.sh
	install_links usbmodem.js /www/luci-static/resources/protocol/usbmodem.js
	install_links acl-proto-usbmodem.json /usr/share/rpcd/acl.d/luci-proto-usbmodem.json
	install_links acl-app-usbmodem.json /usr/share/rpcd/acl.d/luci-app-usbmodem.json
	install_links menu-app-usbmodem.json /usr/share/luci/menu.d/luci-app-usbmodem.json
	install_links usbmodem.usb /etc/hotplug.d/tty/30-usbmodem
	install_links usbmodem /usr/sbin/usbmodem
	
	copy_files
	
	ssh $TEST_ROUTER /etc/init.d/rpcd restart
	ssh $TEST_ROUTER /etc/init.d/network restart
	
	ssh $TEST_ROUTER touch /tmp/usbmodem_ok
	
	echo "done"
	
	exit 0
fi

echo "Send..."
copy_files
