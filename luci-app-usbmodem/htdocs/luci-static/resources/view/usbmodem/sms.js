'use strict';
'require fs';
'require view';
'require rpc';
'require ui';

var SMS_MEMORY_NAMES = {
	SM:		_("SIM"),
	ME:		_("Modem"),
	MT:		_("Modem + SIM")
};

var callInterfaceDump = rpc.declare({
	object: 'network.interface',
	method: 'dump',
	expect: { interface: [] }
});

function callUsbmodem(iface, method, params) {
	var keys = [];
	var values = [];
	
	if (params) {
		for (var k in params) {
			keys.push(k);
			values.push(params[k]);
		}
	}
	
	var callback = rpc.declare({
		object: 'usbmodem.%s'.format(iface),
		method: method,
		params: keys,
	});
	return callback.apply(undefined, values);
}

function progressbar(value, max, byte) {
	var vn = parseInt(value) || 0,
	    mn = parseInt(max) || 100,
	    fv = byte ? String.format('%1024.2mB', value) : value,
	    fm = byte ? String.format('%1024.2mB', max) : max,
	    pc = Math.floor((100 / mn) * vn);

	return E('div', {
		'class': 'cbi-progressbar',
		'title': '%s / %s (%d%%)'.format(fv, fm, pc)
	}, E('div', { 'style': 'width:%.2f%%'.format(pc) }));
}

return view.extend({
	load: function () {
		return callInterfaceDump();
	},
	createInfoTable: function (title, fields) {
		var table = E('table', { 'class': 'table' });
		for (var i = 0; i < fields.length; i += 2) {
			table.appendChild(E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td left', 'width': '33%' }, [ fields[i] ]),
				E('td', { 'class': 'td left' }, [ fields[i + 1] ])
			]));
		}
		
		if (title) {
			return E('div', {}, [ E('h3', {}, [ title ]), table ]);
		} else {
			return E('div', {}, [ table ]);
		}
	},
	onTabSelected: function (iface) {
		var self = this;
		self.active_tab = iface;
		self.updateSmsList(iface, true);
	},
	updateSmsList: function (iface, show_spinner) {
		var self = this;
		
		var sms_list = document.querySelector('#usbmodem-sms-' + iface);
		
		if (show_spinner) {
			sms_list.innerHTML = '';
			sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Loading status...')));
		}
		
		callUsbmodem(self.active_tab, 'read_sms').then(function (result) {
			sms_list.innerHTML = '';
			
			var used_sms_pct = result.capacity.used / result.capacity.total * 100;
			var storage_progressbar = E('div', {
				'class': 'cbi-progressbar',
				'title': '%d / %d (%d%%)'.format(result.capacity.used, result.capacity.total, used_sms_pct)
			}, E('div', { 'style': 'width:%.2f%%'.format(used_sms_pct) }));
			
			sms_list.appendChild(self.createInfoTable(false, [
				_('SMS Memory'), SMS_MEMORY_NAMES[result.storage],
				_('Capacity'), storage_progressbar
			]));
			
			if (result.messages.length > 0) {
				
			} else {
				sms_list.appendChild(E('p', {}, _('No messages.')));
			}
			
			console.log(result);
		}).catch(function (err) {
			console.error(err);
			
			sms_list.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				sms_list.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
				sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
			} else {
				sms_list.appendChild(E('p', { "class": "alert-message error" }, err.message));
			}
		});
	},
	render: function (interfaces) {
		var self = this;
		
		var usbmodems = interfaces.filter(function (v) {
			return v.proto == "usbmodem";
		});
		
		var tabs = [];
		usbmodems.forEach(function (modem) {
			tabs.push(E('div', {
				'data-tab': modem.interface,
				'data-tab-title': modem.interface,
				'cbi-tab-active': ui.createHandlerFn(self, 'onTabSelected', modem.interface)
			}, [
				E('div', {'id': 'usbmodem-sms-' + modem.interface}, [])
			]));
		});
		
		var view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
