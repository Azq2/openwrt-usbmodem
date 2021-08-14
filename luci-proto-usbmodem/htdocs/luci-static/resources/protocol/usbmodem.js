'use strict';
'require rpc';
'require uci';
'require form';
'require network';
'require fs';
'require ui';

network.registerPatternVirtual(/^usbmodem-.+$/);

function loadUsbDevices(option) {
	return new Promise(function (resovle, reject) {
		fs.exec("/usr/sbin/usbmodem", ["discover"]).then(function (res) {
			var json = JSON.parse(res.stdout.trim());
			
			option.value('', _('-- Please choose --'), '');
			for (var i = 0, l = json.modems.length; i < l; i++) {
				var modem = json.modems[i];
				
				modem.tty.sort(function (a, b) {
					return a.interface - b.interface;
				});
				
				for (var j = 0, l = modem.tty.length; i < l; i++) {
					var tty = modem.tty[i];
					
					var descr;
					if (tty.interface_name) {
						descr = tty.interface_name + ' (' + tty.device + ')';
					} else {
						descr = tty.device;
					}
					
					option.value(tty.path, tty.path, ' - ' + modem.name, descr);
				}
			}
			
			json.tty.sort();
			for (var i = 0, l = json.tty.length; i < l; i++) {
				var tty = json.tty[i];
				option.value(tty, tty, '', '');
			}
			
			resovle();
		}).catch(reject);
	});
}

var RichListValue = form.Value.extend({
	value: function(value, title, subtitle, description) {
		if (description) {
			form.Value.prototype.value.call(this, value, E([], [
				E('span', { 'class': 'hide-open' }, [ title ]),
				E('div', { 'class': 'hide-close', 'style': 'min-width:25vw' }, [
					E('strong', [ title + (subtitle || '')]),
					E('br'),
					E('span', { 'style': 'white-space:normal' }, (description || ''))
				])
			]));
		} else {
			form.Value.prototype.value.call(this, value, title);
		}
	}
});

return network.registerProtocol('usbmodem', {
	getI18n: function () {
		return _('USB Modem Service');
	},
	getIfname: function () {
		return this._ubus('l3_device') || 'usbmodem-%s'.format(this.sid);
	},
	getOpkgPackage: function () {
		return 'usbmodem';
	},
	isFloating: function () {
		return true;
	},
	isVirtual: function () {
		return true;
	},
	getDevices: function () {
		return null;
	},
	containsDevice: function (ifname) {
		return (network.getIfnameOf(ifname) == this.getIfname());
	},
	renderFormOptions: function (s) {
		// Device
		var usb_dev = s.taboption('general', RichListValue, 'modem_device', _('Device'));
		usb_dev.ucioption = 'modem_device';
		usb_dev.optional = false;
		usb_dev.default = '';
		usb_dev.load = function () {
			return loadUsbDevices(this);
		};
		usb_dev.cfgvalue = function (section_id) {
			return uci.get('network', section_id, 'modem_device');
		};
		usb_dev.validate = function (section_id, value) {
			if (value == '')
				return _('Please choose device.');
			return true;
		};
		
		// Modem type
		var device_type = s.taboption('general', form.ListValue, 'modem_type', _('Modem type'));
		device_type.ucioption = 'modem_type';
		device_type.optional = false;
		device_type.value('', _('-- Please choose --'));
		device_type.value('asr1802', _('ASR1802 LTE module'));
		device_type.value('ncm', _('NCM (Huawei)'));
		device_type.default = '';
		device_type.validate = function (section_id, value) {
			if (value == '')
				return _('Please choose device type.');
			return true;
		};
		
		// PDP type
		var auth_type = s.taboption('general', form.ListValue, 'pdp_type', _('PDP type'));
		auth_type.ucioption = 'pdp_type';
		auth_type.optional = false;
		auth_type.value('IP', _('IP'));
		auth_type.value('IPV6', _('IPV6'));
		auth_type.value('IPV4V6', _('IPV4V6'));
		auth_type.default = 'IP';
		
		// APN
		var apn = s.taboption('general', form.Value, 'apn', _('APN'));
		apn.default = 'internet';
		apn.validate = function (section_id, value) {
			if (/^\s+$/.test(value))
				return _('Please choose APN.');
			if (!/^[\w\d._-]+$/i.test(value))
				return _('Invalid APN');
			return true;
		};
		
		// PIN code
		var pin = s.taboption('general', form.Value, 'pincode', _('PIN'));
		pin.datatype = 'and(uinteger,minlength(4),maxlength(8))';
		pin.rmempty = true;
		
		// Auth type
		var auth_type = s.taboption('general', form.ListValue, 'auth_type', _('Auth type'));
		auth_type.ucioption = 'auth_type';
		auth_type.optional = false;
		auth_type.value('', _('None'));
		auth_type.value('pap', _('PAP'));
		auth_type.value('chap', _('CHAP'));
		auth_type.default = '';
		auth_type.rmempty = true;
		
		// Auth username
		var auth_username = s.taboption('general', form.Value, 'username', _('Username'));
		auth_username.ucioption = 'username';
		auth_username.depends('auth_type', 'pap');
		auth_username.depends('auth_type', 'chap');
		auth_username.rmempty = true;
		
		// Auth password
		var auth_password = s.taboption('general', form.Value, 'password', _('Password'));
		auth_password.ucioption = 'password';
		auth_password.password = true;
		auth_password.depends('auth_type', 'pap');
		auth_password.depends('auth_type', 'chap');
		auth_password.rmempty = true;
		
		// DHCP
		var force_use_dhcp = s.taboption(
			'advanced',
			form.Flag,
			'force_use_dhcp',
			_('Force use DHCP'),
			_('By default used manual interface configuration using information from modem (fastest way). But fallback to DHCP also available (for worth case).')
		);
		force_use_dhcp.depends('modem_type', 'ncm');
		force_use_dhcp.depends('modem_type', 'asr1802');
		
		// Flight mode
		var allow_flight_mode = s.taboption(
			'advanced',
			form.Flag,
			'allow_flight_mode',
			_('Allow flight mode'),
			_('Allow entering to flight mode after interface down. Otherwise modem can stay in network.')
		);
		allow_flight_mode.default = '1';
		allow_flight_mode.depends('modem_type', 'asr1802');
		
		// Network restart
		var force_network_restart = s.taboption(
			'advanced',
			form.Flag,
			'force_network_restart',
			_('Force network restart'),
			_('Triggers flight mode for few time on first run. May be useful for some modems, but slowdown connect time after reboot.')
		);
		force_use_dhcp.depends('modem_type', 'asr1802');
	}
});