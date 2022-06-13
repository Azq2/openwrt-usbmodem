include $(TOPDIR)/rules.mk

PKG_NAME:=usbmodem
PKG_RELEASE:=1
PKG_LICENSE:=GPLv2

ifneq ($(CONFIG_DEBUG),)
	CMAKE_OPTIONS += -DCMAKE_BUILD_TYPE=Debug
else
	CMAKE_OPTIONS += -DCMAKE_BUILD_TYPE=Release
endif

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/usbmodem
	SECTION:=net
	CATEGORY:=Network
	SUBMENU:=WWAN
	DEPENDS:=+libubus +libstdcpp +libubox +libuci +zlib
	
	ifneq ($(CONFIG_DEBUG),)
		DEPENDS +=+libasan +libubsan +liblsan +libtsan
	endif
	
	PKG_BUILD_DEPENDS:=nlohmannjson
	TITLE:=USB Modem Service
endef

define Package/usbmodem/description
	Support various USB modems (internet, sms, ussd and etc).
	Provides persistent service (RIL-daemon like) which ensure automatic connection to internet.
	Also it provides ubus API for LuCI gui or cli tools.
endef

define Package/usbmodem/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/hotplug.d/tty
	$(INSTALL_DIR) $(1)/lib/netifd/proto
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/usbmodem $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usbmodem.sh $(1)/lib/netifd/proto/usbmodem.sh
	$(INSTALL_BIN) ./files/usbmodem.usb $(1)/etc/hotplug.d/tty/30-usbmodem
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d
	$(INSTALL_DATA) ./files/usbmodem.upgrade $(1)/lib/upgrade/keep.d/usbmodem
endef

$(eval $(call BuildPackage,usbmodem))
