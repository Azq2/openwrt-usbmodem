'use strict';
'require usbmodem';
'require fs';
'require view';
'require ui';

return view.extend({
	load() {
		return usbmodem.getInterfaces();
	},
	onTabSelected(iface) {
		this.active_tab = iface;
	},
	sendCommand() {
		let self = this;
		let form = document.querySelector('#atcmd-form-' + this.active_tab);
		let result_body = form.querySelector('.js-atcmd-result');
		let command = form.querySelector('.js-atcmd').value;
		
		usbmodem.renderSpinner(result_body, _('Waiting for response...'));
		
		usbmodem.call(this.active_tab, 'send_command', { command, async: true }).then((result) => {
			result_body.innerHTML = '';
			result_body.appendChild(E('pre', { }, result.response));
		}).catch((err) => {
			usbmodem.renderApiError(result_body, err);
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
		let view = E('div', {}, [
			usbmodem.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
				return this.renderForm(iface);
			})
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
