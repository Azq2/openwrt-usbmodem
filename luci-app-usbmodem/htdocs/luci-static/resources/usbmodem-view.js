'require ui';
'require baseclass';

let deferred_cnt = 0;

return baseclass.extend({
	renderSignal(type, value) {
		let signals_types = {
			// 2G (GSM)
			rssi_2g:	[_('dBm'), [-109, -100, -85, -70]],
			
			// 3G (UMTS)
			rssi_3g:	[_('dBm'), [-109, -100, -85, -70]],
			ecio:		[_('dB'), [-20, -15, -10, -6]],
			rscp:		[_('dBm'), [-95, -85, -75, -60]],
			
			// 4G (LTE)
			rssi_4g:	[_('dBm'), [-95, -85, -75, -65]],
			rsrp:		[_('dBm'), [-99, -95, -90, -80]],
			rsrq:		[_('dB'), [-19, -17, -15, -10]],
			sinr:		[_('dB'), [0, 6, 13, 20]],
		};
		
		let signal_cfg = signals_types[type];
		if (!signal_cfg)
			return +value.toFixed(2);
		
		let scale = signal_cfg[1];
		let icon = L.resource('icons/signal-0.png');
		
		if (value >= scale[3]) {
			icon = L.resource('icons/signal-75-100.png');
		} else if (value >= scale[2]) {
			icon = L.resource('icons/signal-50-75.png');
		} else if (value >= scale[1]) {
			icon = L.resource('icons/signal-25-50.png');
		} else if (value >= scale[0]) {
			icon = L.resource('icons/signal-0-25.png');
		}
		
		let title = +value.toFixed(2) + ' ' + signal_cfg[0];
		return E('span', { class: 'ifacebadge' }, [
			E('img', { src: icon, title: title }),
			E('span', { }, [ title ])
		]);
	},
	showBusyWarning(flag) {
		if (flag) {
			deferred_cnt++;
		} else {
			deferred_cnt--;
		}
		
		let div = document.querySelector('#busy-message');
		if (!div)
			return;
		
		if (deferred_cnt > 0) {
			this.renderError(div, E('div', { 'class': 'spinning' }, [
				_('The modem currently is not responding, because he processing a heavy operation. Please wait...')
			]), "warning");
		} else {
			div.innerHTML = '';
		}
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
			this.renderError(div, E('div', { 'class': 'spinning' }, [
				_('Modem not found. Please insert your modem to USB.')
			]), "warning");
		} else {
			this.renderError(div, err.message);
		}
	},
	renderError(div, error, severity) {
		severity = severity || "error";
		div.innerHTML = '';
		div.appendChild(E('p', { "class": "alert-message " + severity }, error));
	},
	renderSpinner(div, message) {
		div.innerHTML = '';
		div.appendChild(E('p', { 'class': 'spinning' }, message));
	},
	renderSubTabs(tabs_names, current, callback) {
		let tabs = [];
		for (let id in tabs_names) {
			tabs.push(E('li', { 'class': id == current ? 'cbi-tab' : 'cbi-tab-disabled' }, [
				E('a', {
					'href': '#',
					'click': ui.createHandlerFn(callback[0], callback[1], id)
				}, [ tabs_names[id] ])
			]));
		}
		return E('ul', { 'class': 'cbi-tabmenu' }, tabs);
	},
	renderModemTabs(interfaces, callback, render) {
		let view = E('div', {}, [
			// E('div', { id: 'busy-message' }, []),
			E('div', { id: 'global-error' }, []),
			E('div', { }, interfaces.map((modem) => {
				return E('div', {
					'data-tab': modem.interface,
					'data-tab-title': modem.interface,
					'cbi-tab-active': ui.createHandlerFn(callback[0], callback[1], modem.interface)
				}, [
					render(modem.interface)
				]);
			}))
		]);
		
		document.querySelector('head').appendChild(E('link', {
			'rel': 'stylesheet',
			'type': 'text/css',
			'href': L.resource('view/usbmodem/custom.css')
		}));
		
		ui.tabs.initTabGroup(view.querySelectorAll('[data-tab]'));
		return view;
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
			(title ? E('h3', {}, [ title ]) : ''),
			table
		]);
	},
	renderNetIndicator(quality, title) {
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
	}
});
