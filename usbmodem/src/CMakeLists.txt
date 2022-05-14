cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(usbmodem)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")

add_executable(usbmodem
	main.cpp
	
	Modem.cpp
	Modem/BaseAt.cpp
	Modem/Asr1802.cpp
	ModemService.cpp
	ModemServiceApi.cpp
	
	Core/Serial.cpp
	Core/AtChannel.cpp
	Core/Utils.cpp
	Core/Events.cpp
	Core/GsmUtils.cpp
	Core/AtParser.cpp
	Core/BinaryParser.cpp
	Core/UsbDiscover.cpp
	Core/Ubus.cpp
	Core/Blobmsg.cpp
	Core/UbusObject.cpp
	Core/UbusRequest.cpp
	Core/Netifd.cpp
	Core/Loop.cpp
	Core/Uci.cpp
)
target_include_directories(usbmodem PUBLIC .)
target_link_libraries(usbmodem -lubox -lubus -luci -lstdc++ -lstdc++fs -lz)
install(TARGETS usbmodem DESTINATION sbin/)

# target_precompile_headers(usbmodem PUBLIC Json.h)
