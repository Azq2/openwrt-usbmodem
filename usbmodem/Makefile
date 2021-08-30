include $(TOPDIR)/rules.mk

PKG_NAME:=usbmodem
PKG_RELEASE:=1
PKG_LICENSE:=GPLv2

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/usbmodem
	SECTION:=net
	CATEGORY:=Network
	SUBMENU:=WWAN
	DEPENDS:=+libubus +libstdcpp +libubox +libuci +nlohmannjson +zlib
	TITLE:=USB Modem Service
endef

define Package/usbmodem/description
	Rethinking of USB modems support in openwrt.
endef

define Package/usbmodem/install
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usbmodem $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/usbmodem.sh $(1)/lib/netifd/proto/usbmodem.sh
	$(INSTALL_BIN) ./files/usbmodem.usb $(1)/etc/hotplug.d/tty/30-usbmodem
endef

$(eval $(call BuildPackage,usbmodem))
