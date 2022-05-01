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
	cancelUssd() {
		let form = document.querySelector('#ussd-form-' + this.active_tab);
		form.querySelector('.js-ussd-result').innerHTML = '';
		usbmodem.call(this.active_tab, 'cancel_ussd', {});
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
		
		usbmodem.call(this.active_tab, 'send_ussd', params).then((result) => {
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
		let view = E('div', {}, [
			usbmodem.createModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
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
