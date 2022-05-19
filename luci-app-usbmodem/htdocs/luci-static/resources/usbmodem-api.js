'require rpc';
'require poll';
'require baseclass';
'require usbmodem-view';

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
								if (response.result.error)
									throw new Error(response.result.error);
								
								usbmodem_view.showBusyWarning(false);
								poll.remove(poll_result_fn);
								resolve(response.result);
							} else if (!response.exists) {
								throw new Error('Deferred result expired');
							}
						}).catch((e) => {
							usbmodem_view.showBusyWarning(false);
							poll.remove(poll_result_fn);
							reject(e);
						});
					};
					
					usbmodem_view.showBusyWarning(true);
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
	}
});
