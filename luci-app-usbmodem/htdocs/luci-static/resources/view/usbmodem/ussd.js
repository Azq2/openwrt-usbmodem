'use strict';
'require fs';
'require view';
'require rpc';
'require ui';

let callInterfaceDump = rpc.declare({
	object: 'network.interface',
	method: 'dump',
	expect: { interface: [] }
});

function callUsbmodem(iface, method, params) {
	let keys = [];
	let values = [];
	
	if (params) {
		for (let k in params) {
			keys.push(k);
			values.push(params[k]);
		}
	}
	
	let callback = rpc.declare({
		object: 'usbmodem.%s'.format(iface),
		method: method,
		params: keys,
	});
	return callback.apply(undefined, values);
}

return view.extend({
	load() {
		return callInterfaceDump();
	},
	onTabSelected(iface) {
		this.active_tab = iface;
	},
	cancelUssd() {
		let form = document.querySelector('#ussd-form-' + this.active_tab);
		form.querySelector('.js-ussd-result').innerHTML = '';
		callUsbmodem(this.active_tab, 'cancel_ussd', {});
	},
	sendUssd(is_answer) {
		let form = document.querySelector('#ussd-form-' + this.active_tab);
		let result_body = form.querySelector('.js-ussd-result');
		
		let params = {};
		if (is_answer) {
			params.answer = form.querySelector('.js-ussd-answer').value;
		} else {
			params.query = form.querySelector('.js-ussd-query').value;
		}
		
		result_body.innerHTML = '';
		result_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for response...')));
		
		callUsbmodem(this.active_tab, 'send_ussd', params).then((result) => {
			result_body.innerHTML = '';
			if (result.error) {
				result_body.appendChild(E('p', { "class": "alert-message error" }, result.error));
			} else {
				if (!result.response && result.code == 2) {
					result_body.appendChild(E('p', { "class": "alert-message error" }, _('Discard by network.')));
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
						"click": ui.createHandlerFn(this, 'sendUssd', true)
					}, [_("Send answer")]),
					E('div', { "style": "padding-top: 1em" }, [
						E('button', {
							"class": "btn cbi-button cbi-button-reset",
							"click": ui.createHandlerFn(this, 'cancelUssd')
						}, [_("Cancel session")])
					])
				]));
			}
		}).catch((err) => {
			result_body.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				result_body.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
			} else {
				result_body.appendChild(E('p', {}, err.message));
			}
		});
	},
	renderForm(iface) {
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
					"click": ui.createHandlerFn(this, 'sendUssd', false)
				}, [_("Send code")])
			]),
			E('div', { "class": "js-ussd-result", "style": "padding-top: 1em" }, [])
		]);
	},
	render(interfaces) {
		let usbmodems = interfaces.filter((v) => {
			return v.proto == "usbmodem";
		});
		
		let tabs = [];
		usbmodems.forEach((modem) => {
			tabs.push(E('div', {
				'data-tab': modem.interface,
				'data-tab-title': modem.interface,
				'cbi-tab-active': ui.createHandlerFn(this, 'onTabSelected', modem.interface)
			}, [
				this.renderForm(modem.interface)
			]));
		});
		
		let view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
