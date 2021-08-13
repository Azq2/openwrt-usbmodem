# USB Modem Service
Rethinking of USB modems support in openwrt.

# Usage
1. Add to **feeds.conf.default**
```
src-git usbmodem https://github.com/Azq2/openwrt-usbmodem
```
2. Init feed
```
./scripts/feeds update usbmodem
./scripts/feeds install -a -p usbmodem
```
3. Select packages in menuconfig
