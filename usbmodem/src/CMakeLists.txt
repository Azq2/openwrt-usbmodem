cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 20)

project(usbmodem)

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s -Os -Wl,--gc-sections -fdata-sections -ffunction-sections -flto")

add_executable(usbmodem
	main.cpp
	
	Modem.cpp
	
	Modem/BaseAt.cpp
	Modem/BaseAt/Info.cpp
	Modem/BaseAt/Network.cpp
	Modem/BaseAt/Sms.cpp
	Modem/BaseAt/Sim.cpp
	Modem/BaseAt/Ussd.cpp
	
	Modem/Asr1802.cpp
	Modem/Asr1802/Data.cpp
	Modem/Asr1802/Network.cpp
	
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
	Core/LoopBase.cpp
	Core/UbusLoop.cpp
	Core/Uci.cpp
	Core/Semaphore.cpp
)
target_include_directories(usbmodem PUBLIC .)
target_link_libraries(usbmodem -lubox -lubus -luci -lstdc++ -lstdc++fs -lz)
install(TARGETS usbmodem DESTINATION sbin/)

# target_precompile_headers(usbmodem PUBLIC Json.h)
