'use strict';
'require usbmodem';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

return view.extend({
	load() {
		return usbmodem.api.getInterfaces();
	},
	onTabSelected(iface) {
		this.iface = iface;
		return this.updateStatus();
	},
	updateStatus() {
		if (!this.iface)
			return Promise.resolve();
		
		let promises = [
			usbmodem.api.call(this.iface, 'getModemInfo'),
			usbmodem.api.call(this.iface, 'getSimInfo'),
			usbmodem.api.call(this.iface, 'getNetworkInfo')
		];
		return Promise.all(promises).then(([modem, sim, network]) => {
			usbmodem.view.clearError();
			
			let status_body = document.querySelector(`#usbmodem-status-${this.iface}`);
			status_body.innerHTML = '';
			status_body.appendChild(this.renderStatusTable(modem, sim, network));
		}).catch((e) => {
			usbmodem.view.showApiError(e);
		});
	},
	renderStatusTable(modem, sim, network) {
		let section_modem = [
			_("Modem"), modem.vendor + " " + modem.model,
			_("Software"), modem.version,
			_("IMEI"), modem.imei
		];
		
		let net_section = [
			_('Network registration'), usbmodem.NET_REG[network.registration],
			_('Network type'), usbmodem.TECH[network.tech]
		];
		
		if (network.operator.registration != "NONE")
			net_section.push(_('Operator name'), network.operator.name + (network.operator.registration == 'MANUAL' ? _(' (manual)') : ''));
		
		if (network.signal.rssi_dbm !== null) {
			let title = _('%s dBm (%d%%)').format(network.signal.rssi_dbm, network.signal.quality);
			net_section.push(_("RSSI"), usbmodem.view.renderNetIndicator(network.signal.quality, title));
		}
		if (network.signal.rscp_dbm !== null)
			net_section.push(_("RSCP"), _('%s dBm').format(network.signal.rscp_dbm));
		if (network.signal.rsrp_dbm !== null)
			net_section.push(_("RSRP"), _('%s dBm').format(network.signal.rsrp_dbm));
		if (network.signal.rsrq_db !== null)
			net_section.push(_("RSRQ"), _('%s dB').format(network.signal.rsrq_db));
		if (network.signal.ecio_db !== null)
			net_section.push(_("Ec/io"), _('%s dB').format(network.signal.ecio_db));
		if (network.signal.bit_err_pct !== null)
			net_section.push(_("Bit errors"), '%s%%'.format(network.signal.bit_err_pct));
		
		let cell_section = [];
		if (network.operator.registration != "NONE") {
			cell_section.push(_('MCC / MNC'), '%03d / %02d'.format(network.operator.mcc, network.operator.mnc));
			
			if (network.cell.loc_id || network.cell.cell_id) {
				cell_section.push(_('LAC'), '%d (%x)'.format(network.cell.loc_id, network.cell.loc_id));
				cell_section.push(_('CID'), '%d (%x)'.format(network.cell.cell_id, network.cell.cell_id));
			}
		}
		
		let ipv4_section = [];
		if (network.ipv4.ip) {
			ipv4_section.push(_("IP"), network.ipv4.ip);
			
			ipv4_section.push(_("Gateway"), network.ipv4.gw || "0.0.0.0");
			ipv4_section.push(_("Netmask"), network.ipv4.mask || "0.0.0.0");
			
			if (network.ipv4.dns1)
				ipv4_section.push(_("DNS1"), network.ipv4.dns1);
			if (network.ipv4.dns2)
				ipv4_section.push(_("DNS2"), network.ipv4.dns2);
		}
		
		let ipv6_section = [];
		if (network.ipv6.ip) {
			ipv6_section.push(_("IP"), network.ipv6.ip);
			
			ipv6_section.push(_("Gateway"), network.ipv6.gw || "0000:0000:0000:0000:0000:0000:0000:0000");
			ipv6_section.push(_("Netmask"), network.ipv6.mask || "0000:0000:0000:0000:0000:0000:0000:0000");
			
			if (network.ipv6.dns1)
				ipv6_section.push(_("DNS1"), network.ipv6.dns1);
			if (network.ipv6.dns2)
				ipv6_section.push(_("DNS2"), network.ipv6.dns2);
		}
		
		let sim_section = [];
		if (sim.state) {
			if (usbmodem.api.isSimError(sim.state)) {
				sim_section.push(_("Status"), E('div', { 'class': 'alert-message error' }, [
					usbmodem.SIM_STATE[sim.state]
				]));
			} else {
				sim_section.push(_("Status"), usbmodem.SIM_STATE[sim.state]);
			}
		}
		if (sim.number)
			sim_section.push(_("Phone number"), sim.number);
		if (sim.imsi)
			sim_section.push(_("IMSI"), sim.imsi);
		
		return E('div', {}, [
			section_modem.length > 0 ? usbmodem.view.renderTable(_('Modem'), section_modem) : '',
			net_section.length > 0 ? usbmodem.view.renderTable(_('Network'), net_section) : '',
			cell_section.length > 0 ? usbmodem.view.renderTable(_('Cell'), cell_section) : '',
			sim_section.length > 0 ? usbmodem.view.renderTable(_('SIM'), sim_section) : '',
			ipv4_section.length > 0 ? usbmodem.view.renderTable(_('IPv4'), ipv4_section) : '',
			ipv6_section.length > 0 ? usbmodem.view.renderTable(_('IPv6'), ipv6_section) : '',
		]);
	},
	render(interfaces) {
		poll.add(() => {
			return this.updateStatus();
		});
		poll.start();
		
		return usbmodem.view.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
			return E('div', { id: `usbmodem-status-${iface}` }, []);
		});
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
