'require rpc';
'require ui';
'require poll';
'require baseclass';
'require usbmodem-view';
'require usbmodem-api';

return baseclass.extend({
	NET_REG: {
		NOT_REGISTERED:			_("Unregistered"),
		SEARCHING:				_("Searching network..."),
		REGISTERED_HOME:		_("Home network"),
		REGISTERED_ROAMING:		_("Roaming network"),
	},
	TECH: {
		UNKNOWN:		_("Unknown"),
		NO_SERVICE:		_("No service"),
		GSM:			_("GSM"),
		GSM_COMPACT:	_("GSM Compact"),
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
		ERROR:				_("Error"),
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
