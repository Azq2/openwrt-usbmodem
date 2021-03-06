'require rpc';
'require ui';
'require poll';
'require baseclass';
'require usbmodem-view';
'require usbmodem-api';

return baseclass.extend({
	NET_MODES: {
		"AUTO":				_("Auto"),
		
		"ONLY_2G":			_("Only 2G"),
		"ONLY_3G":			_("Only 3G"),
		"ONLY_4G":			_("Only 4G"),
		
		"PREFER_2G":		_("Prefer 2G"),
		"PREFER_3G":		_("Prefer 3G"),
		"PREFER_4G":		_("Prefer 4G"),
		
		"2G_3G_AUTO":		_("2G/3G (auto)"),
		"2G_3G_PREFER_2G":	_("2G/3G (prefer 2G)"),
		"2G_3G_PREFER_3G":	_("2G/3G (prefer 2G)"),
		
		"2G_4G_AUTO":		_("2G/4G (auto)"),
		"2G_4G_PREFER_2G":	_("2G/4G (prefer 2G)"),
		"2G_4G_PREFER_4G":	_("2G/4G (prefer 3G)"),
		
		"3G_4G_AUTO":		_("3G/4G (auto)"),
		"3G_4G_PREFER_3G":	_("3G/4G (prefer 3G)"),
		"3G_4G_PREFER_4G":	_("3G/4G (prefer 4G)"),
	},
	NET_REG: {
		NOT_REGISTERED:			_("Unregistered"),
		SEARCHING:				_("Searching network..."),
		REGISTERED_HOME:		_("Home network"),
		REGISTERED_ROAMING:		_("Roaming network"),
		EMERGENY_ONLY:			_("Emergency only"),
	},
	TECH: {
		UNKNOWN:		_("Unknown"),
		NO_SERVICE:		_("No service"),
		GSM:			_("2G (GSM)"),
		GSM_COMPACT:	_("2G (GSM Compact)"),
		GPRS:			_("2G (GPRS)"),
		EDGE:			_("2G (EDGE)"),
		UMTS:			_("3G (UMTS)"),
		HSDPA:			_("3G (HSDPA)"),
		HSUPA:			_("3G (HSUPA)"),
		HSPA:			_("3G (HSPA)"),
		HSPAP:			_("3G (HSPA+)"),
		LTE:			_("4G (LTE)"),
	},
	SIM_STATE: {
		NOT_INITIALIZED:	_("Not initialized"),
		NOT_SUPPORTED:		_("Not supported by Modem"),
		READY:				_("Ready"),
		PIN1_LOCK:			_("Locked - Need PIN1 code for unlock"),
		PIN2_LOCK:			_("Locked - Need PIN2 code for unlock"),
		PUK1_LOCK:			_("Locked - Need PUK1 code for unlock"),
		PUK2_LOCK:			_("Locked - Need PUK2 code for unlock"),
		MEP_LOCK:			_("Locked - Need MEP/NCK code for unlock"),
		OTHER_LOCK:			_("Locked - Meed UNKNOWN code for unlock"),
		WAIT_UNLOCK:		_("Unlocking..."),
		ERROR:				_("SIM not present or failure"),
		REMOVED:			_("SIM removed")
	},
	OPERATOR_STATUS: {
		UNKNOWN:	_('Unknown'),
		AVAILABLE:	_('Available'),
		REGISTERED:	_('Registered'),
		FORBIDDEN:	_('Forbidden'),
	},
	
	view:	usbmodem_view,
	api:	usbmodem_api
});
