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
		params = params || {};
		params.async = true;
		
		return new Promise((resolve, reject) => {
			this._call(iface, method, params).then((response) => {
				if (response.deferred) {
					let deferred_id = response.deferred;
					let poll_result_fn = () => {
						return this._call(iface, 'getDeferredResult', {id: deferred_id}).then((response) => {
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
	},
	renderTable(title, fields) {
		let table = E('table', { 'class': 'table' });
		for (let i = 0; i < fields.length; i += 2) {
			table.appendChild(E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td left', 'width': '33%' }, [ fields[i] ]),
				E('td', { 'class': 'td left' }, [ fields[i + 1] ])
			]));
		}
		return E('div', {}, [
			E('h3', {}, [ title ]),
			table
		]);
	},
	isSimError(state) {
		switch (state) {
			case "PIN1_LOCK":	return true;
			case "PIN2_LOCK":	return true;
			case "PUK1_LOCK":	return true;
			case "PUK2_LOCK":	return true;
			case "MEP_LOCK":	return true;
			case "OTHER_LOCK":	return true;
			case "ERROR":		return true;
		}
		return false;
	},
	createNetIndicator(quality, title) {
		let icon;
		if (quality < 0)
			icon = L.resource('icons/signal-none.png');
		else if (quality <= 0)
			icon = L.resource('icons/signal-0.png');
		else if (quality < 25)
			icon = L.resource('icons/signal-0-25.png');
		else if (quality < 50)
			icon = L.resource('icons/signal-25-50.png');
		else if (quality < 75)
			icon = L.resource('icons/signal-50-75.png');
		else
			icon = L.resource('icons/signal-75-100.png');
		
		return E('span', { class: 'ifacebadge' }, [
			E('img', { src: icon, title: title || '' }),
			E('span', { }, [ title ])
		]);
	},
});
