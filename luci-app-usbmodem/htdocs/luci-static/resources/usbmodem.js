'require rpc';
'require ui';
'require poll';
'require baseclass';

return baseclass.extend({
	_call(iface, method, params) {
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
	},
	call(iface, method, params) {
		return new Promise((resolve, reject) => {
			this._call(iface, method, params).then((response) => {
				if (response.deferred) {
					let deferred_id = response.deferred;
					let poll_result_fn = () => {
						return this._call(iface, 'get_deferred_result', {id: deferred_id}).then((response) => {
							if (response.ready) {
								poll.remove(poll_result_fn);
								
								if (response.result.error)
									throw new Error(response.result.error);
								
								resolve(response.result);
							} else if (!response.exists) {
								throw new Error('Deferred result expired');
							}
						}).catch(reject);
					};
					poll.add(poll_result_fn);
					poll.start();
				} else {
					if (response.error)
						throw new Error(response.error);
					resolve(response);
				}
			}).catch(reject);
		});
	},
	getInterfaces() {
		let callback = rpc.declare({
			object: 'network.interface',
			method: 'dump',
			expect: { interface: [] }
		});
		return callback().then((result) => {
			return result.filter((v) => v.proto == "usbmodem");
		});
	},
	clearError() {
		let div = document.querySelector('#global-error');
		div.innerHTML = '';
	},
	showMessage(text, type) {
		let div = document.querySelector('#global-error');
		div.innerHTML = '';
		div.appendChild(E('p', { "class": "alert-message " + (type || 'success') }, text));
	},
	showApiError(err) {
		let div = document.querySelector('#global-error');
		return this.renderApiError(div, err);
	},
	renderApiError(div, err) {
		div.innerHTML = '';
		if (err.message.indexOf('Object not found') >= 0) {
			div.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
			div.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
		} else {
			this.renderError(div, err.message);
		}
	},
	renderError(div, error) {
		div.innerHTML = '';
		div.appendChild(E('p', { "class": "alert-message error" }, error));
	},
	renderSpinner(div, message) {
		div.innerHTML = '';
		div.appendChild(E('p', { 'class': 'spinning' }, message));
	},
	renderModemTabs(interfaces, callback, render) {
		return E('div', {}, interfaces.map((modem) => {
			return E('div', {
				'data-tab': modem.interface,
				'data-tab-title': modem.interface,
				'cbi-tab-active': ui.createHandlerFn(callback[0], callback[1], modem.interface)
			}, [
				render(modem.interface)
			]);
		}));
	}
});
