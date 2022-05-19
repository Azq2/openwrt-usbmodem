'use strict';
'require usbmodem';
'require fs';
'require view';
'require ui';

return view.extend({
	load() {
		return usbmodem.api.getInterfaces();
	},
	onTabSelected(iface) {
		this.iface = iface;
	},
	sendCommand() {
		let self = this;
		let form = document.querySelector(`#atcmd-form-${this.iface}`);
		let result_body = form.querySelector('.js-atcmd-result');
		let command = form.querySelector('.js-atcmd').value;
		
		usbmodem.view.renderSpinner(result_body, _('Waiting for response...'));
		
		usbmodem.api.call(this.iface, 'sendCommand', { command }).then((result) => {
			result_body.innerHTML = '';
			result_body.appendChild(E('pre', { }, [
				result.response
			]));
		}).catch((err) => {
			usbmodem.view.renderApiError(result_body, err);
		});
	},
	renderForm(iface) {
		return E('div', { "id": `atcmd-form-${iface}` }, [
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
		return usbmodem.view.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
			return this.renderForm(iface);
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
