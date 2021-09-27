'use strict';
'require fs';
'require view';
'require rpc';
'require ui';
'require poll';

var NETWORK_STATUSES = {
	"not_registered":	_("Not registered"),
	"searching":		_("Searching network..."),
	"home":				_("Home network"),
	"roaming":			_("Roaming network"),
};

var callInterfaceDump = rpc.declare({
	object: 'network.interface',
	method: 'dump',
	expect: { interface: [] }
});

function callUsbmodem(iface, method, params) {
	var keys = [];
	var values = [];
	
	if (params) {
		for (var k in params) {
			keys.push(k);
			values.push(params[k]);
		}
	}
	
	var callback = rpc.declare({
		object: 'usbmodem.%s'.format(iface),
		method: method,
		params: keys,
	});
	return callback.apply(undefined, values);
}

return view.extend({
	load: function () {
		return callInterfaceDump();
	},
	createInfoTable: function (title, fields) {
		var table = E('table', { 'class': 'table' });
		for (var i = 0; i < fields.length; i += 2) {
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
	createNetIndicator: function (quality, title) {
		var icon;
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
	updateStatus: function (iface, show_spinner) {
		var self = this;
		self.current_iface = iface;
		
		var status_body = document.querySelector('#usbmodem-status-' + iface);
		
		if (show_spinner) {
			status_body.innerHTML = '';
			status_body.appendChild(E('p', { 'class': 'spinning' }, _('Loading status...')));
		}
		
		callUsbmodem(iface, 'info').then(function (result) {
			var section_modem = [
				_("Modem"), result.modem.vendor + " " + result.modem.model,
				_("Software"), result.modem.version,
				_("IMEI"), result.modem.imei,
			];
			
			var net_section = [
				_('Network registration'), NETWORK_STATUSES[result.network_status.name],
				_('Network type'), result.tech.name
			];
			if (result.levels.rssi_dbm !== null) {
				var title = _('%s dBm (%d%%)').format(result.levels.rssi_dbm, result.levels.quality);
				net_section.push(_("RSSI"), self.createNetIndicator(result.levels.quality, title));
			}
			if (result.levels.rscp_dbm !== null)
				net_section.push(_("RSCP"), _('%s dBm').format(result.levels.rscp_dbm));
			if (result.levels.rsrp_dbm !== null)
				net_section.push(_("RSRP"), _('%s dBm').format(result.levels.rsrp_dbm));
			if (result.levels.rsrq_db !== null)
				net_section.push(_("RSRQ"), _('%s dB').format(result.levels.rsrq_db));
			if (result.levels.eclo_db !== null)
				net_section.push(_("Ec/lo"), _('%s dB').format(result.levels.eclo_db));
			if (result.levels.bit_err_pct !== null)
				net_section.push(_("Bit errors"), '%s%%'.format(result.levels.bit_err_pct));
			
			var ipv4_section = [];
			if (result.ipv4.ip) {
				ipv4_section.push(_("IP"), result.ipv4.ip);
				
				ipv4_section.push(_("Gateway"), result.ipv4.gw || "0.0.0.0");
				ipv4_section.push(_("Netmask"), result.ipv4.mask || "0.0.0.0");
				
				if (result.ipv4.dns1)
					ipv4_section.push(_("DNS1"), result.ipv4.dns1);
				if (result.ipv4.dns2)
					ipv4_section.push(_("DNS2"), result.ipv4.dns2);
			}
			
			var ipv6_section = [];
			if (result.ipv6.ip) {
				ipv6_section.push(_("IP"), result.ipv6.ip);
				
				ipv6_section.push(_("Gateway"), result.ipv6.gw || "0000:0000:0000:0000:0000:0000:0000:0000");
				ipv6_section.push(_("Netmask"), result.ipv6.mask || "0000:0000:0000:0000:0000:0000:0000:0000");
				
				if (result.ipv6.dns1)
					ipv6_section.push(_("DNS1"), result.ipv6.dns1);
				if (result.ipv6.dns2)
					ipv6_section.push(_("DNS2"), result.ipv6.dns2);
			}
			
			var sim_section = [];
			if (result.sim.number)
				sim_section.push(_("Phone number"), result.sim.number);
			if (result.sim.imsi)
				sim_section.push(_("IMSI"), result.sim.imsi);
			
			status_body.innerHTML = '';
			
			if (section_modem.length > 0)
				status_body.appendChild(self.createInfoTable(_('Modem'), section_modem));
			if (net_section.length > 0)
				status_body.appendChild(self.createInfoTable(_('Network'), net_section));
			if (sim_section.length > 0)
				status_body.appendChild(self.createInfoTable(_('SIM'), sim_section));
			if (ipv4_section.length > 0)
				status_body.appendChild(self.createInfoTable(_('IPv4'), ipv4_section));
			if (ipv6_section.length > 0)
				status_body.appendChild(self.createInfoTable(_('IPv6'), ipv6_section));
		}).catch(function (err) {
			console.error(err);
			
			status_body.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				status_body.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
				status_body.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
			} else {
				status_body.appendChild(E('p', { "class": "alert-message error" }, err.message));
			}
		});
	},
	render: function (interfaces) {
		var self = this;
		
		var usbmodems = interfaces.filter(function (v) {
			return v.proto == "usbmodem";
		});
		
		var tabs = [];
		usbmodems.forEach(function (modem) {
			tabs.push(E('div', {
				'data-tab': modem.interface,
				'data-tab-title': modem.interface,
				'cbi-tab-active': ui.createHandlerFn(self, 'updateStatus', modem.interface, true)
			}, [
				E('div', {
					'id': 'usbmodem-status-' + modem.interface
				}, [])
			]));
		});
		
		var view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		poll.add(function () {
			if (self.current_iface)
				self.updateStatus(self.current_iface, false);
			return Promise.resolve();
		});
		poll.start();
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
