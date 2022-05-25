#!/bin/bash
# Dev script for test on real router without rebuilding openwrt
# Example usage:
# TEST_ROUTER=root@192.168.1.1 TOPDIR=~/build/openwrt ./test.sh

VERSION=16
DIR=$(readlink -f $0)
DIR=$(dirname $0)
MODE=$1

if [[ $TOPDIR == "" ]]; then
	# Openwrt build root
	export TOPDIR=~/openwrt
fi

if [[ $TEST_ROUTER == "" ]]; then
	# Test router
	#TEST_ROUTER=root@192.168.2.1
	TEST_ROUTER=root@192.168.122.91
fi

if [[ $TARGET == "" ]]; then
	#export TARGET=target-mipsel_24kc_musl
	#export TARGET=target-x86_64_musl
	export TARGET=target-x86_64_glibc
fi

export CONFIG_DEBUG=y

export PATH="$PATH:$TOPDIR/staging_dir/host/bin"

echo "MODE=$MODE"

# Build package
echo "Build..."
if [[ $MODE == "build" ]]; then
	make -C "$DIR/luci-proto-usbmodem" -j9 compile || exit 1
	make -C "$DIR/luci-proto-usbmodem" -j9 install || exit 1

	make -C "$DIR/luci-app-usbmodem" -j9 compile || exit 1
	make -C "$DIR/luci-app-usbmodem" -j9 install || exit 1
fi

make -C "$DIR/usbmodem" -j9 compile || exit 1
make -C "$DIR/usbmodem" -j9 install || exit 1

if [[ $MODE == "build" ]]; then
	echo "Build done."
	exit 0
fi

if [[ $MODE == "test" ]]; then
	ssh $TEST_ROUTER killall -9 /tmp/usbmodem-test
	ssh $TEST_ROUTER killall -9 /tmp/usbmodem-test
	ssh $TEST_ROUTER killall -9 /tmp/usbmodem-test
	scp -r $TOPDIR/build_dir/$TARGET/usbmodem/ipkg-install/usr/sbin/usbmodem "$TEST_ROUTER:/tmp/usbmodem-test"
	echo ""
	echo ""
	echo ""
	ssh $TEST_ROUTER /tmp/usbmodem-test test
	exit 0
fi

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
install_file $DIR/luci-app-usbmodem/htdocs/luci-static/resources/usbmodem-view.js			/www/luci-static/resources/usbmodem-view.js
install_file $DIR/luci-app-usbmodem/htdocs/luci-static/resources/usbmodem-api.js			/www/luci-static/resources/usbmodem-api.js
install_file $DIR/luci-app-usbmodem/htdocs/luci-static/resources/view/usbmodem				/www/luci-static/resources/view/usbmodem
install_file $DIR/luci-proto-usbmodem/htdocs/luci-static/resources/protocol/usbmodem.js		/www/luci-static/resources/protocol/usbmodem.js

install_file $DIR/luci-proto-usbmodem/root/usr/share/rpcd/acl.d/luci-proto-usbmodem.json	/usr/share/rpcd/acl.d/luci-proto-usbmodem.json
install_file $DIR/luci-app-usbmodem/root/usr/share/rpcd/acl.d/luci-app-usbmodem.json		/usr/share/rpcd/acl.d/luci-app-usbmodem.json
install_file $DIR/luci-app-usbmodem/root/usr/share/luci/menu.d/luci-app-usbmodem.json		/usr/share/luci/menu.d/luci-app-usbmodem.json

install_file $DIR/usbmodem/files/usbmodem.sh												/lib/netifd/proto/usbmodem.sh
install_file $DIR/usbmodem/files/usbmodem.user												/etc/usbmodem.sh
install_file $DIR/usbmodem/files/usbmodem.usb												/etc/hotplug.d/tty/30-usbmodem

ssh $TEST_ROUTER killall -9 usbmodem
install_file $TOPDIR/build_dir/$TARGET/usbmodem/ipkg-install/usr/sbin/usbmodem					/usr/sbin/usbmodem

if [[ $NEED_SETUP != "0" ]];
then
	echo "Need setup..."
	ssh $TEST_ROUTER /etc/init.d/rpcd restart
	ssh $TEST_ROUTER /etc/init.d/network restart
	ssh $TEST_ROUTER touch /tmp/usbmodem_ok.$VERSION
	exit 0
fi
