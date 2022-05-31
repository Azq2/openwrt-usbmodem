'use strict';
'require usbmodem';
'require fs';
'require view';
'require ui';
'require poll';

return view.extend({
	/*
	 * Monitor
	 * */
	updateNeighboringCells(show_spinner) {
		let list = document.getElementById('cells-list');
		if (show_spinner)
			usbmodem.view.renderSpinner(list, _('Loading...'));
		
		return usbmodem.api.call(this.iface, 'getNeighboringCell').then((result) => {
			list.innerHTML = '';
			list.appendChild(this.renderNeighboringCells(result.list));
		}).catch((err) => {
			usbmodem.view.showApiError(err);
		});
	},
	renderMonitor() {
		poll.add(() => this.updateNeighboringCells());
		poll.start();
		
		setTimeout(() => {
			this.updateNeighboringCells(true);
		}, 0);
		
		return E('div', { }, [
			E('div', { }, [
				E('h4', { }, [ _('Neighboring cells') ]),
				E('div', { 'id': 'cells-list', 'style': 'padding-top: 1em' }, [ ])
			]),
		]);
	},
	renderNeighboringCells(list) {
		let table = E('table', { 'class': 'table cbi-section-table' }, [
			E('tr', { 'class': 'tr cbi-section-table-titles' }, [
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Cell ID') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('LAC') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('MCC / MNC') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('RSSI') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('RSCP') ]),
				E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Arfcn') ])
			])
		]);
		
		for (let row of list) {
			let code = '%03d / %02d'.format(row.mcc, row.mnc);
			table.appendChild(E('tr', { 'class': 'tr cbi-section-table-row' }, [
				E('td', { 'class': 'td cbi-value-field left top' }, [ "%d (%x)".format(row.cell_id, row.cell_id) ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ "%d (%x)".format(row.loc_id, row.loc_id) ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ code ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ !row.rssi_dbm ? '?' : _('%s dBm').format(row.rssi_dbm) ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ !row.rscp_dbm ? '?' : _('%s dBm').format(row.rscp_dbm) ]),
				E('td', { 'class': 'td cbi-value-field left top' }, [ row.freq ]),
			]));
		}
		
		return table;
	},
	
	/*
	 * AT Commands
	 * */
	sendCommand() {
		let self = this;
		let form = document.querySelector(`#atcmd-form-${this.iface}`);
		let result_body = form.querySelector('.js-atcmd-result');
		let command = form.querySelector('.js-atcmd').value;
		
		usbmodem.view.renderSpinner(result_body, _('Waiting for response...'));
		
		usbmodem.api.call(this.iface, 'sendCommand', { command }).then((result) => {
			result_body.innerHTML = '';
			result_body.appendChild(E('pre', { }, [
				result.response
			]));
		}).catch((err) => {
			usbmodem.view.renderApiError(result_body, err);
		});
	},
	renderCmd() {
		return E('div', { "id": `atcmd-form-${this.iface}` }, [
			E('div', {}, [
				E('input', {
					"type": "text",
					"value": "",
					"placeholder": "AT",
					"class": "js-atcmd",
					"style": "margin-bottom: 5px"
				}),
				E('button', {
					"class": "btn cbi-button cbi-button-apply",
					"click": ui.createHandlerFn(this, 'sendCommand')
				}, [_("Execute")])
			]),
			E('div', { "class": "js-atcmd-result", "style": "padding-top: 1em" }, [])
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
			cmd:		_('AT'),
			monitor:	_('Cell Monitor')
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
		
		let form = document.getElementById('tab-content');
		form.innerHTML = '';
		form.appendChild(this.renderForm());
	},
	render(interfaces) {
		return usbmodem.view.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
			this.iface = iface;
			this.tab = this.tab || 'cmd';
			return E('div', { id: 'tab-content' }, [
				this.renderForm()
			]);
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
