'use strict';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

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

return view.extend({
	load: function () {
		return callInterfaceDump();
	},
	onTabSelected: function (iface) {
		var self = this;
		self.active_tab = iface;
	},
	sendCommand: function () {
		var self = this;
		var form = document.querySelector('#atcmd-form-' + self.active_tab);
		var result_body = form.querySelector('.js-atcmd-result');
		var command = form.querySelector('.js-atcmd').value;
		
		result_body.innerHTML = '';
		result_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for response...')));
		
		callUsbmodem(self.active_tab, 'send_command', { "command": command }).then(function (result) {
			result_body.innerHTML = '';
			result_body.appendChild(E('pre', { }, result.response));
		}).catch(function (err) {
			result_body.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				result_body.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
			} else {
				result_body.appendChild(E('p', {}, err.message));
			}
		});
	},
	renderForm: function (iface) {
		var self = this;
		
		return E('div', { "id": "atcmd-form-" + iface }, [
			E('div', {}, [
				E('input', {
					"type": "text",
					"value": "",
					"placeholder": "AT",
					"class": "js-atcmd"
				}),
				E('button', {
					"class": "btn cbi-button cbi-button-apply",
					"click": ui.createHandlerFn(self, 'sendCommand')
				}, [_("Execute")])
			]),
			E('div', { "class": "js-atcmd-result", "style": "padding-top: 1em" }, [])
		]);
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
				self.renderForm(modem.interface)
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
