cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(usbmodem)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")

add_executable(usbmodem main.cpp Serial.cpp AtChannel.cpp Utils.cpp AtParser.cpp Modem.cpp UsbDiscover.cpp ModemService.cpp Ubus.cpp Netifd.cpp Loop.cpp Modem/Asr1802.cpp)
target_link_libraries(usbmodem -lubox -lubus -luci -lstdc++ -lstdc++fs -ljson-c -lblobmsg_json)
install(TARGETS usbmodem DESTINATION sbin/)
