'use strict';
'require usbmodem';
'require fs';
'require view';
'require ui';
'require poll';

return view.extend({
	load() {
		return usbmodem.getInterfaces();
	},
	onTabSelected(iface) {
		this.iface = iface;
		this.updateStatus();
	},
	joinNetwork(id, tech) {
		return usbmodem.call(this.iface, 'set_operator', {id: id, tech: tech}).then((result) => {
			console.log(result);
		});
	},
	searchNetworks(e) {
		let network_list = document.getElementById('network-list');
		network_list.innerHTML = '';
		
		return usbmodem.call(this.iface, 'search_operators', {async: true}).then((result) => {
			let network_list = document.getElementById('network-list');
			network_list.innerHTML = '';
			network_list.appendChild(this.renderNetworkList(result.list));
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
		
		for (let i = 0; i < list.length; i++) {
			let row = list[i];
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
			E('div', { }, [
				E('h4', { }, [ _('Network search') ]),
				
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
				]),
				E('div', { 'id': 'network-list', 'style': 'padding-top: 1em' }, [ ])
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
