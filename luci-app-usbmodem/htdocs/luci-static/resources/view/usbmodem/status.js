'use strict';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

let NETWORK_STATUSES = {
	"not_registered":	_("Not registered"),
	"searching":		_("Searching network..."),
	"home":				_("Home network"),
	"roaming":			_("Roaming network"),
};

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
	createInfoTable(title, fields) {
		let table = E('table', { 'class': 'table' });
		for (let i = 0; i < fields.length; i += 2) {
			table.appendChild(E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td left', 'width': '33%' }, [ fields[i] ]),
				E('td', { 'class': 'td left' }, [ fields[i + 1] ])
			]));
		}
		return E('div', {}, [
			E('h3', {}, [ title ]),
			table
		]);
	},
	createNetIndicator(quality, title) {
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
	},
	updateStatus(iface, show_spinner) {
		this.current_iface = iface;
		
		let status_body = document.querySelector('#usbmodem-status-' + iface);
		
		if (show_spinner) {
			status_body.innerHTML = '';
			status_body.appendChild(E('p', { 'class': 'spinning' }, _('Loading status...')));
		}
		
		callUsbmodem(iface, 'info').then((result) => {
			let section_modem = [
				_("Modem"), result.modem.vendor + " " + result.modem.model,
				_("Software"), result.modem.version,
				_("IMEI"), result.modem.imei
			];
			
			let net_section = [
				_('Network registration'), NETWORK_STATUSES[result.network_status.name],
				_('Network operator'), '%s - %s%s'.format(result.operator.id, result.operator.name, (result.operator.registration == 'manual' ? _(' (manual)') : '')),
				_('Network type'), result.tech.name
			];
			if (result.levels.rssi_dbm !== null) {
				let title = _('%s dBm (%d%%)').format(result.levels.rssi_dbm, result.levels.quality);
				net_section.push(_("RSSI"), this.createNetIndicator(result.levels.quality, title));
			}
			if (result.levels.rscp_dbm !== null)
				net_section.push(_("RSCP"), _('%s dBm').format(result.levels.rscp_dbm));
			if (result.levels.rsrp_dbm !== null)
				net_section.push(_("RSRP"), _('%s dBm').format(result.levels.rsrp_dbm));
			if (result.levels.rsrq_db !== null)
				net_section.push(_("RSRQ"), _('%s dB').format(result.levels.rsrq_db));
			if (result.levels.ecio_db !== null)
				net_section.push(_("Ec/io"), _('%s dB').format(result.levels.ecio_db));
			if (result.levels.bit_err_pct !== null)
				net_section.push(_("Bit errors"), '%s%%'.format(result.levels.bit_err_pct));
			
			let ipv4_section = [];
			if (result.ipv4.ip) {
				ipv4_section.push(_("IP"), result.ipv4.ip);
				
				ipv4_section.push(_("Gateway"), result.ipv4.gw || "0.0.0.0");
				ipv4_section.push(_("Netmask"), result.ipv4.mask || "0.0.0.0");
				
				if (result.ipv4.dns1)
					ipv4_section.push(_("DNS1"), result.ipv4.dns1);
				if (result.ipv4.dns2)
					ipv4_section.push(_("DNS2"), result.ipv4.dns2);
			}
			
			let ipv6_section = [];
			if (result.ipv6.ip) {
				ipv6_section.push(_("IP"), result.ipv6.ip);
				
				ipv6_section.push(_("Gateway"), result.ipv6.gw || "0000:0000:0000:0000:0000:0000:0000:0000");
				ipv6_section.push(_("Netmask"), result.ipv6.mask || "0000:0000:0000:0000:0000:0000:0000:0000");
				
				if (result.ipv6.dns1)
					ipv6_section.push(_("DNS1"), result.ipv6.dns1);
				if (result.ipv6.dns2)
					ipv6_section.push(_("DNS2"), result.ipv6.dns2);
			}
			
			let sim_section = [];
			if (result.sim.number)
				sim_section.push(_("Phone number"), result.sim.number);
			if (result.sim.imsi)
				sim_section.push(_("IMSI"), result.sim.imsi);
			
			status_body.innerHTML = '';
			
			if (section_modem.length > 0)
				status_body.appendChild(this.createInfoTable(_('Modem'), section_modem));
			if (net_section.length > 0)
				status_body.appendChild(this.createInfoTable(_('Network'), net_section));
			if (sim_section.length > 0)
				status_body.appendChild(this.createInfoTable(_('SIM'), sim_section));
			if (ipv4_section.length > 0)
				status_body.appendChild(this.createInfoTable(_('IPv4'), ipv4_section));
			if (ipv6_section.length > 0)
				status_body.appendChild(this.createInfoTable(_('IPv6'), ipv6_section));
		}).catch((err) => {
			status_body.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				status_body.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
				status_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
			} else {
				status_body.appendChild(E('p', { "class": "alert-message error" }, err.message));
			}
		});
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
				'cbi-tab-active': ui.createHandlerFn(this, 'updateStatus', modem.interface, true)
			}, [
				E('div', {
					'id': 'usbmodem-status-' + modem.interface
				}, [])
			]));
		});
		
		let view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		poll.add(() => {
			if (this.current_iface)
				this.updateStatus(this.current_iface, false);
			return Promise.resolve();
		});
		poll.start();
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
