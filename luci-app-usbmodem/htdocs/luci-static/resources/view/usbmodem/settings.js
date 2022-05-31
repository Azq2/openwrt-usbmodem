'use strict';
'require usbmodem';
'require fs';
'require view';
'require ui';
'require poll';

let NETWORK_MODES = {
	'auto':					_('Auto'),
	
	'only_2g':				_('Only 2G'),
	'only_3g':				_('Only 3G'),
	'only_4g':				_('Only 4G'),
	
	'prefer_2g':			_('Prefer 2G'),
	'prefer_3g':			_('Prefer 3G'),
	'prefer_4g':			_('Prefer 4G'),
	
	'2g_3g_auto':			_('2G/3G (auto)'),
	'2g_3g_prefer_2g':		_('2G/3G (prefer 2G)'),
	'2g_3g_prefer_3g':		_('2G/3G (prefer 3G)'),
	
	'2g_4g_auto':			_('2G/4G (auto)'),
	'2g_4g_prefer_2g':		_('2G/4G (prefer 2G)'),
	'2g_4g_prefer_4g':		_('2G/4G (prefer 4G)'),
	
	'3g_4g_auto':			_('3G/4G (auto)'),
	'3g_4g_prefer_3g':		_('3G/4G (prefer 43)'),
	'3g_4g_prefer_4g':		_('3G/4G (prefer 4G)'),
	
	'2g_3g_4g_auto':		_('2G/3G/4G (auto)'),
	'2g_3g_4g_prefer_2g':	_('2G/3G/4G (prefer 2G)'),
	'2g_3g_4g_prefer_3g':	_('2G/3G/4G (prefer 3G)'),
	'2g_3g_4g_prefer_4g':	_('2G/3G/4G (prefer 4G)'),
};

return view.extend({
	/*
	 * Security
	 * */
	renderSecurity() {
		
	},
	
	/*
	 * Network
	 * */
	saveNetworkMode(e) {
		let params = {
			roaming:	document.querySelector('#network-roaming').checked,
			mode:		document.querySelector('#network-mode').value
		};
		
		return usbmodem.api.call(this.iface, 'setNetworkSettings', params).then((result) => {
			usbmodem.view.showMessage(_('Network mode saved'));
			this.updateNetworkSettings();
		}).catch((err) => {
			usbmodem.view.showApiError(err);
		});
	},
	updateNetworkSettings() {
		return usbmodem.api.call(this.iface, 'getNetworkSettings').then((result) => {
			document.querySelector('#network-roaming').checked = result.roaming;
			
			let network_mode_select = document.querySelector('#network-mode');
			network_mode_select.innerHTML = '';
			
			network_mode_select.appendChild(E('option', {}, [_('--- Not selected ---')]));
			for (let mode of result.network_modes) {
				let option = E('option', { value: mode }, [
					usbmodem.NET_MODES[mode] || mode
				]);
				option.selected = (result.network_mode == mode);
				network_mode_select.appendChild(option);
			}
		}).catch((err) => {
			usbmodem.view.showApiError(err);
		});
	},
	updateNetworkInfo() {
		return usbmodem.api.call(this.iface, 'getNetworkInfo').then((network) => {
			let current_operator = document.getElementById('current-operator');
			let network_reg = document.getElementById('network-reg');
			
			if (network.registration == "NOT_REGISTERED" || network.registration == "SEARCHING") {
				current_operator.innerHTML = _('Not selected');
				network_reg.innerHTML = usbmodem.NET_REG[network.registration];
			} else {
				let code = '%03d%02d'.format(network.operator.mcc, network.operator.mnc);
				if (network.tech == "NO_SERVICE") {
					current_operator.innerHTML = network.operator.name + ' / ' + code;
				} else{
					current_operator.innerHTML = network.operator.name + ' ' + usbmodem.TECH[network.tech] + ' / ' + code;
				}
				network_reg.innerHTML = usbmodem.NET_REG[network.registration] + ' ' + (network.operator.registration == 'MANUAL' ? _(' (manual)') : ' (auto)');
			}
		});
	},
	searchNetworks(e) {
		usbmodem.view.clearError();
		
		let network_list = document.getElementById('network-list');
		network_list.innerHTML = '';
		
		return usbmodem.api.call(this.iface, 'searchOperators', {}).then((result) => {
			let network_list = document.getElementById('network-list');
			network_list.innerHTML = '';
			network_list.appendChild(this.renderNetworkList(result.list));
		}).catch((err) => {
			usbmodem.view.showApiError(err);
		});
	},
	joinNetwork(mode, mcc, mnc, tech) {
		return usbmodem.api.call(this.iface, 'setOperator', {mode, mcc, mnc, tech}).then((result) => {
			if (result.success) {
				usbmodem.view.showMessage(_('New operator selected.'));
			} else {
				usbmodem.view.showMessage(_('Failed to select new operator.'), 'error');
			}
		}).catch((err) => {
			usbmodem.view.showApiError(err);
		});
	},
	renderNetworkList(list) {
		let table = E('table', { 'class': 'table cbi-section-table' }, [
			E('tr', { 'class': 'tr cbi-section-table-titles' }, [
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Code') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Name') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Status') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ ])
			])
		]);
		
		for (let row of list) {
			let code = '%03d%02d'.format(row.mcc, row.mnc);
			table.appendChild(E('tr', { 'class': 'tr cbi-section-table-row' }, [
				E('td', { 'class': 'td cbi-value-field left top' }, [ code ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ row.name + ' ' + usbmodem.TECH[row.tech] ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ usbmodem.OPERATOR_STATUS[row.status] ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [
					E('button', {
						"class": "btn cbi-button cbi-button-apply",
						"click": ui.createHandlerFn(this, 'joinNetwork', 'manual', row.mcc, row.mnc, row.tech)
					}, [_("Join")]),
				]),
			]));
		}
		
		return table;
	},
	renderNetwork() {
		this.updateNetworkInfo();
		this.updateNetworkSettings();
		
		poll.add(() => this.updateNetworkInfo());
		poll.start();
		
		return E('div', { }, [
			E('div', { }, [
				usbmodem.view.renderTable(false, [
					_('Current operator'), E('span', { 'id': 'current-operator' }, [ '?' ]),
					_('Network registration'), E('span', { 'id': 'network-reg' }, [ '?' ])
				]),
				
				E('h4', { }, [ _('Network selection') ]),
				E('div', { }, [
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						"click": ui.createHandlerFn(this, 'joinNetwork', 'auto')
					}, [ _('Auto') ]),
					' ',
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						'click': ui.createHandlerFn(this, 'searchNetworks')
					}, [ _('Search networks') ]),
					' ',
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						'data-type': 'none',
						"click": ui.createHandlerFn(this, 'joinNetwork', 'none')
					}, [ _('Deregister') ])
				]),
				
				E('div', { 'id': 'network-list', 'style': 'padding-top: 1em' }, [ ]),
				
				// Mode
				E('h4', { }, [ _('Network mode') ]),
				
				E('div', { 'style': 'padding-bottom: 10px' }, [
					E('select', { 'id': 'network-mode' }, [ ])
				]),
				
				E('div', { 'style': 'padding-bottom: 10px' }, [
					E('label', { }, [
						E('input', { 'id': 'network-roaming', 'type': 'checkbox', 'style': 'margin-right:0.5em' }, [ ]),
						_('Enable data roaming')
					])
				]),
				
				E('div', { }, [
					E('button', {
						'class': 'btn cbi-button cbi-button-save',
						"click": ui.createHandlerFn(this, 'saveNetworkMode')
					}, [ _('Save') ])
				]),
			]),
		]);
	},
	
	/*
	 * Common
	 * */
	load() {
		return usbmodem.api.getInterfaces();
	},
	renderForm() {
		let tabs = {
			network:	_('Network'),
			security:	_('Security'),
			sms:		_('SMS'),
		};
		
		let tab_render_func = 'render' + (this.tab.substr(0, 1).toUpperCase() + this.tab.substr(1));
		
		return E('div', { }, [
			usbmodem.view.renderSubTabs(tabs, this.tab, [this, 'onSubTabSelected']),
			this[tab_render_func]()
		]);
	},
	onTabSelected(iface) {
		this.iface = iface;
	},
	onSubTabSelected(tab) {
		this.tab = tab;
		
		let form = document.getElementById('settings-form');
		form.innerHTML = '';
		form.appendChild(this.renderForm());
	},
	render(interfaces) {
		return usbmodem.view.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
			this.iface = iface;
			this.tab = this.tab || 'network';
			return E('div', { id: 'settings-form' }, [
				this.renderForm()
			]);
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
