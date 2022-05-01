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
	load() {
		return usbmodem.getInterfaces();
	},
	onTabSelected(iface) {
		this.iface = iface;
		this.updateStatus();
		this.updateSettings();
	},
	joinNetwork(id, tech) {
		usbmodem.clearError();
		return usbmodem.call(this.iface, 'set_operator', {id: id, tech: tech}).then(() => {
			usbmodem.showMessage(_('New operator selected'));
		}).catch((err) => {
			usbmodem.showApiError(err);
		});
	},
	saveNetworkMode(e) {
		usbmodem.clearError();
		
		let params = {
			async:		true,
			roaming:	document.querySelector('#network-roaming').checked,
			mode:		document.querySelector('#network-mode').value
		};
		
		return usbmodem.call(this.iface, 'set_network_mode', params).then((result) => {
			usbmodem.showMessage(_('Network mode saved'));
			this.updateSettings();
		}).catch((err) => {
			usbmodem.showApiError(err);
		});
	},
	searchNetworks(e) {
		usbmodem.clearError();
		
		let network_list = document.getElementById('network-list');
		network_list.innerHTML = '';
		
		return usbmodem.call(this.iface, 'search_operators', {async: true}).then((result) => {
			let network_list = document.getElementById('network-list');
			network_list.innerHTML = '';
			network_list.appendChild(this.renderNetworkList(result.list));
		}).catch((err) => {
			usbmodem.showApiError(err);
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
		
		let STATUS_NAMES = {
			available:	_('Available'),
			registered:	_('Registered'),
			forbidden:	_('Forbidden'),
		};
		
		for (let row of list) {
			table.appendChild(E('tr', { 'class': 'tr cbi-section-table-row' }, [
				E('td', { 'class': 'td cbi-value-field left top' }, [ row.id ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ row.name + ' ' + row.tech.name ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ STATUS_NAMES[row.status] || row.status ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [
					E('button', {
						"class": "btn cbi-button cbi-button-apply",
						"click": ui.createHandlerFn(this, 'joinNetwork', row.id, row.tech.id)
					}, [_("Join")]),
				]),
			]));
		}
		
		return table;
	},
	updateSettings() {
		return usbmodem.call(this.iface, 'get_settings').then((result) => {
			document.querySelector('#network-roaming').checked = result.roaming_enabled;
			
			let network_mode_select = document.querySelector('#network-mode');
			network_mode_select.innerHTML = '';
			
			network_mode_select.appendChild(E('option', {}, [_('--- Not selected ---')]));
			for (let mode of result.network_modes) {
				let option = E('option', { 'value': mode.id }, [
					NETWORK_MODES[mode.name] || mode.name
				]);
				option.selected = result.network_mode == mode.id;
				network_mode_select.appendChild(option);
			}
		});
	},
	updateStatus() {
		if (!this.iface)
			return Promise.resolve();
		
		return usbmodem.call(this.iface, 'info').then((result) => {
			let current_network = document.getElementById('current-network');
			current_network.innerHTML = '%s - %s %s%s'.format(
				result.operator.id, result.operator.name, result.tech.name, 
				(result.operator.registration == 'manual' ? _(' (manual)') : '')
			);
		});
	},
	renderForm(iface) {
		return E('div', { }, [
			E('div', { 'id': 'global-error' }, [ ]),
			
			E('div', { }, [
				// Search
				E('h4', { }, [ _('Network searching') ]),
				
				E('div', { 'style': 'padding-bottom: 10px' }, [
					E('b', { }, [ _('Current network:') ]),
					' ',
					E('span', { 'id': 'current-network' }, [ '?' ]),
				]),
				
				E('div', { }, [
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						'data-type': 'auto',
						"click": ui.createHandlerFn(this, 'joinNetwork', 'auto', 0)
					}, [ _('Auto') ]),
					' ',
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						'data-type': 'manual',
						'click': ui.createHandlerFn(this, 'searchNetworks')
					}, [ _('Search networks') ]),
					' ',
					E('button', {
						'class': 'btn cbi-button js-network-reg',
						'data-type': 'none',
						"click": ui.createHandlerFn(this, 'joinNetwork', 'none', 0)
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
						'data-type': 'auto',
						"click": ui.createHandlerFn(this, 'saveNetworkMode')
					}, [ _('Save') ])
				]),
			]),
		]);
	},
	render(interfaces) {
		let view = E('div', {}, [
			usbmodem.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
				return this.renderForm(iface);
			})
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		poll.add(() => this.updateStatus());
		poll.start();
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
