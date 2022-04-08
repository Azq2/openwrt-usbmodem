'use strict';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

let callInterfaceDump = rpc.declare({
	object: 'network.interface',
	method: 'dump',
	expect: { interface: [] }
});

function callUsbmodem(iface, method, params) {
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
}

return view.extend({
	load() {
		return callInterfaceDump();
	},
	onTabSelected(iface) {
		this.active_tab = iface;
		this.updateStatus();
	},
	joinNetwork(id, tech) {
		return callUsbmodem(this.active_tab, 'set_operator', {id: id, tech: tech}).then((result) => {
			console.log(result);
		});
	},
	changeNetworkSelection(e) {
		let network_list = document.getElementById('network-list');
		network_list.innerHTML = '';
		
		return callUsbmodem(this.active_tab, 'search_operators').then(() => {
			return new Promise((resolve, reject) => {
				this.search_operators_resolve = resolve;
			});
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
		let promises = [];
		promises.push(callUsbmodem(this.active_tab, 'info').then((result) => {
			let current_network = document.getElementById('current-network');
			current_network.innerHTML = '%s - %s %s%s'.format(
				result.operator.id, result.operator.name, result.tech.name, 
				(result.operator.registration == 'manual' ? _(' (manual)') : '')
			);
		}));
		if (this.search_operators_resolve) {
			promises.push(callUsbmodem(this.active_tab, 'search_operators_result').then((result) => {
				if (!result.searching) {
					if (this.search_operators_resolve) {
						this.search_operators_resolve();
						this.search_operators_resolve = false;
					}
					
					let network_list = document.getElementById('network-list');
					network_list.innerHTML = '';
					network_list.appendChild(this.renderNetworkList(result.list));
				}
			}));
		}
		return Promise.all(promises);
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
						'click': ui.createHandlerFn(this, 'changeNetworkSelection')
					}, [ _('Search networks') ]),
				]),
				E('div', { 'id': 'network-list', 'style': 'padding-top: 1em' }, [ ])
			]),
		]);
	},
	render(interfaces) {
		let usbmodems = interfaces.filter((v) => {
			return v.proto == "usbmodem";
		});
		
		let tabs = [];
		usbmodems.forEach((modem) => {
			tabs.push(E('div', {
				'data-tab': modem.interface,
				'data-tab-title': modem.interface,
				'cbi-tab-active': ui.createHandlerFn(this, 'onTabSelected', modem.interface)
			}, [
				this.renderForm(modem.interface)
			]));
		});
		
		let view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		poll.add(() => {
			if (this.active_tab)
				this.updateStatus(this.active_tab, false);
			return Promise.resolve();
		});
		poll.start();
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
