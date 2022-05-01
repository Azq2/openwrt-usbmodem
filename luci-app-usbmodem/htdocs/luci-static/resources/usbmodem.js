'require rpc';
'require ui';
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
	checkDeferredResult(deferred_id, poll_result_fn, resolve, reject) {
		console.log('check deferred result');
		return this._call(iface, 'get_deferred_result', {}).then((response) => {
			if (response.result) {
				console.log('has deferred result');
				poll.remove(poll_result_fn);
				resolve(response.result);
			} else {
				console.log('no deferred result....');
			}
		}).catch(reject);			
	},
	call(iface, method, params) {
		return new Promise((resolve, reject) => {
			this._call(iface, method, params).then((response) => {
				if (response.deferred) {
					let poll_result_fn = () => {
						return this.checkDeferredResult(response.deferred, poll_result_fn, resolve, reject);
					};
					poll.add(poll_result_fn);
					poll.start();
				} else {
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
	createModemTabs(interfaces, callback, render) {
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
