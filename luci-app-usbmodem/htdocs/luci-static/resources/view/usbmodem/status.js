'use strict';
'require usbmodem';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

const NET_REG_NAMES = {
	NOT_REGISTERED:			_("Unregistered"),
	SEARCHING:				_("Searching network..."),
	REGISTERED_HOME:		_("Home network"),
	REGISTERED_ROAMING:		_("Roaming network"),
};

const TECH_NAMES = {
	UNKNOWN:		_("Unknown"),
	NO_SERVICE:		_("No service"),
	GSM:			_("GSM"),
	GPRS:			_("2G (GPRS)"),
	EDGE:			_("2G (EDGE)"),
	UMTS:			_("3G (UMTS)"),
	HSDPA:			_("3G (HSDPA)"),
	HSUPA:			_("3G (HSUPA)"),
	HSPA:			_("3G (HSPA)"),
	HSPAP:			_("3G (HSPA+)"),
	LTE:			_("4G (LTE)"),
};

const SIM_STATE_NAMES = {
	NOT_INITIALIZED:	_("Not initialized"),
	NOT_SUPPORTED:		_("Not supported by Modem"),
	READY:				_("Ready"),
	PIN1_LOCK:			_("Locked - Need PIN1 code for unlock"),
	PIN2_LOCK:			_("Locked - Need PIN2 code for unlock"),
	PUK1_LOCK:			_("Locked - Need PUK1 code for unlock"),
	PUK2_LOCK:			_("Locked - Need PUK2 code for unlock"),
	MEP_LOCK:			_("Locked - Need MEP/NCK code for unlock"),
	OTHER_LOCK:			_("Locked - Meed UNKNOWN code for unlock"),
	WAIT_UNLOCK:		_("Unlocking..."),
	ERROR:				_("Error"),
};

return view.extend({
	load() {
		return usbmodem.getInterfaces();
	},
	onTabSelected(iface) {
		this.iface = iface;
		return this.updateStatus();
	},
	updateStatus() {
		if (!this.iface)
			return Promise.resolve();
		
		let promises = [
			usbmodem.call(this.iface, 'getModemInfo'),
			usbmodem.call(this.iface, 'getSimInfo'),
			usbmodem.call(this.iface, 'getNetworkInfo')
		];
		return Promise.all(promises).then(([modem, sim, network]) => {
			let status_body = document.querySelector(`#usbmodem-status-${this.iface}`);
			status_body.innerHTML = '';
			status_body.appendChild(this.renderStatusTable(modem, sim, network));
		});
	},
	renderStatusTable(modem, sim, network) {
		let section_modem = [
			_("Modem"), modem.vendor + " " + modem.model,
			_("Software"), modem.version,
			_("IMEI"), modem.imei
		];
		
		let net_section = [
			_('Network registration'), NET_REG_NAMES[network.registration],
			//_('Network operator'), '%s - %s%s'.format(operator.id, operator.name, (operator.registration == 'manual' ? _(' (manual)') : '')),
			_('Network type'), TECH_NAMES[network.tech]
		];
		if (network.signal.rssi_dbm !== null) {
			let title = _('%s dBm (%d%%)').format(network.signal.rssi_dbm, network.signal.quality);
			net_section.push(_("RSSI"), usbmodem.createNetIndicator(network.signal.quality, title));
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
			if (usbmodem.isSimError(sim.state)) {
				sim_section.push(_("Status"), E('div', { 'class': 'alert-message error' }, [
					SIM_STATE_NAMES[sim.state]
				]));
			} else {
				sim_section.push(_("Status"), SIM_STATE_NAMES[sim.state]);
			}
		}
		if (sim.number)
			sim_section.push(_("Phone number"), sim.number);
		if (sim.imsi)
			sim_section.push(_("IMSI"), sim.imsi);
		
		return E('div', {}, [
			section_modem.length > 0 ? usbmodem.renderTable(_('Modem'), section_modem) : '',
			net_section.length > 0 ? usbmodem.renderTable(_('Network'), net_section) : '',
			sim_section.length > 0 ? usbmodem.renderTable(_('SIM'), sim_section) : '',
			ipv4_section.length > 0 ? usbmodem.renderTable(_('IPv4'), ipv4_section) : '',
			ipv6_section.length > 0 ? usbmodem.renderTable(_('IPv6'), ipv6_section) : '',
		]);
	},
	render(interfaces) {
		let view = E('div', {}, [
			usbmodem.renderModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
				return E('div', { id: `usbmodem-status-${iface}` }, []);
			})
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		poll.add(() => {
			return this.updateStatus();
		});
		poll.start();
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
