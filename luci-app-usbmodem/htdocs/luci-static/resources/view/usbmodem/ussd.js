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
	cancelUssd() {
		let form = document.querySelector('#ussd-form-' + this.iface);
		form.querySelector('.js-ussd-result').innerHTML = '';
		return usbmodem.api.call(this.iface, 'cancelUssd', {});
	},
	sendUssd(is_answer) {
		let form = document.querySelector('#ussd-form-' + this.iface);
		let result_body = form.querySelector('.js-ussd-result');
		
		let params = {async: true};
		if (is_answer) {
			params.answer = form.querySelector('.js-ussd-answer').value;
		} else {
			params.query = form.querySelector('.js-ussd-query').value;
		}
		
		usbmodem.view.renderSpinner(result_body, _('Waiting for response...'));
		
		usbmodem.api.call(this.iface, 'sendUssd', params).then((result) => {
			result_body.innerHTML = '';
			if (result.error) {
				usbmodem.renderError(result_body, result.error);
			} else {
				if (!result.response && result.code == 2) {
					usbmodem.view.renderError(result_body, _('Discard by network.'));
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
			usbmodem.view.renderApiError(result_body, err);
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
		return usbmodem.view.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
			return this.renderForm(iface);
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
