'use strict';
'require fs';
'require view';
'require rpc';
'require ui';

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
	cancelUssd: function () {
		var self = this;
		var form = document.querySelector('#ussd-form-' + self.active_tab);
		form.querySelector('.js-ussd-result').innerHTML = '';
		callUsbmodem(self.active_tab, 'cancel_ussd', {});
	},
	sendUssd: function (is_answer) {
		var self = this;
		var form = document.querySelector('#ussd-form-' + self.active_tab);
		var result_body = form.querySelector('.js-ussd-result');
		
		var params = {};
		if (is_answer) {
			params.answer = form.querySelector('.js-ussd-answer').value;
		} else {
			params.query = form.querySelector('.js-ussd-query').value;
		}
		
		result_body.innerHTML = '';
		result_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for response...')));
		
		callUsbmodem(self.active_tab, 'send_ussd', params).then(function (result) {
			result_body.innerHTML = '';
			if (result.error) {
				result_body.appendChild(E('p', { "class": "alert-message error" }, result.error));
			} else {
				if (!result.response && result.code == 2) {
					result_body.appendChild(E('p', { "class": "alert-message error" }, _('Discard by network. On some ISP ussd codes available only on 3G.')));
				} else {
					result_body.appendChild(E('pre', { }, result.response));
				}
			}
			
			if (result.code == 1) {
				result_body.appendChild(E('div', { }, [
					E('input', {
						"type": "text",
						"value": "",
						"placeholder": "",
						"class": "js-ussd-answer",
						"style": "margin-bottom: 5px"
					}),
					E('button', {
						"class": "btn cbi-button cbi-button-apply",
						"click": ui.createHandlerFn(self, 'sendUssd', true)
					}, [_("Send answer")]),
					E('div', { "style": "padding-top: 1em" }, [
						E('button', {
							"class": "btn cbi-button cbi-button-reset",
							"click": ui.createHandlerFn(self, 'cancelUssd')
						}, [_("Cancel session")])
					])
				]));
			}
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
		
		return E('div', { "id": "ussd-form-" + iface }, [
			E('div', {}, [
				E('input', {
					"type": "text",
					"value": "",
					"placeholder": "*123#",
					"class": "js-ussd-query",
					"style": "margin-bottom: 5px"
				}),
				E('button', {
					"class": "btn cbi-button cbi-button-apply",
					"click": ui.createHandlerFn(self, 'sendUssd', false)
				}, [_("Send code")])
			]),
			E('div', { "class": "js-ussd-result", "style": "padding-top: 1em" }, [])
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
