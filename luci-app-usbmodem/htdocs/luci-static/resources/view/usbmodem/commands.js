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
	sendCommand() {
		let self = this;
		let form = document.querySelector('#atcmd-form-' + this.active_tab);
		let result_body = form.querySelector('.js-atcmd-result');
		let command = form.querySelector('.js-atcmd').value;
		
		result_body.innerHTML = '';
		result_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for response...')));
		
		callUsbmodem(this.active_tab, 'send_command', { "command": command }).then((result) => {
			result_body.innerHTML = '';
			result_body.appendChild(E('pre', { }, result.response));
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
		return E('div', { "id": "atcmd-form-" + iface }, [
			E('div', {}, [
				E('input', {
					"type": "text",
					"value": "",
					"placeholder": "AT",
					"class": "js-atcmd",
					"style": "margin-bottom: 5px"
				}),
				E('button', {
					"class": "btn cbi-button cbi-button-apply",
					"click": ui.createHandlerFn(this, 'sendCommand')
				}, [_("Execute")])
			]),
			E('div', { "class": "js-atcmd-result", "style": "padding-top: 1em" }, [])
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
