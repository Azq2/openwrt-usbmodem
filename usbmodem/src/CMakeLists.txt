cmake_minimum_required(VERSION 3.12)
set(CMAKE_CXX_STANDARD 17)

project(usbmodem)

set(DEBUG_SATINIZE "-fsanitize=address")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_RELEASE} -g -Werror -Wno-switch -Wno-sign-compare -Wall -O1 ${DEBUG_SATINIZE}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_RELEASE} -g -Werror -Wno-switch -Wno-sign-compare -Wall -O1 ${DEBUG_SATINIZE}")

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
	Modem/Asr1802/EngMode.cpp
	
	ModemService.cpp
	ModemServiceApi.cpp
	
	Core/Serial.cpp
	Core/AtChannel.cpp
	Core/Utils.cpp
	Core/Events.cpp
	Core/GsmUtils.cpp
	Core/AtParser.cpp
	Core/BinaryStream.cpp
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
	Core/SmsDb.cpp
	Core/Semaphore.cpp
)
target_include_directories(usbmodem PUBLIC .)
target_link_libraries(usbmodem -lubox -lubus -luci -lstdc++ -lstdc++fs)
install(TARGETS usbmodem DESTINATION sbin/)
# target_precompile_headers(usbmodem PUBLIC Core/Json.h)
