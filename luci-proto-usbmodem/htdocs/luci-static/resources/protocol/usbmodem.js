'use strict';
'require rpc';
'require uci';
'require form';
'require network';
'require fs';
'require ui';

network.registerPatternVirtual(/^usbmodem-.+$/);
network.registerErrorCode('USBMODEM_INTERNAL_ERROR',  _('Internal error, see logread.'));
network.registerErrorCode('USBMODEM_INVALID_CONFIG',  _('Internal configuration, see logread.'));

const MODEM_TYPES = {
	ppp: {
		title:	_('Classic modem (PPP)'),
		fields:	[
			"control_device",
			"ppp_device",
			"net_type",
			"pdp_type",
			"apn",
			"username",
			"password",
			"pin_code"
		]
	},
	ncm: {
		title:	_('Huawei (NCM)'),
		fields:	[
			"control_device",
			"net_device",
			"pdp_type",
			"apn",
			"auth_type",
			"username",
			"password",
			"pin_code",
			"mep_code",
			"prefer_dhcp"
		]
	},
	asr1802: {
		title:	_('Marvell ASR1802'),
		fields:	[
			"control_device",
			"net_device",
			"pdp_type",
			"apn",
			"auth_type",
			"username",
			"password",
			"pin_code",
			"mep_code",
			"prefer_dhcp"
		]
	}
};

const MODEM_FIELDS = {
	// General
	device: {
		type:		'rich_select',
		tab:		'general',
		title:		_('Modem'),
		descr:		_('This list contains plugged USB modems. Choose one for automatic setup.'),
		optional:	false,
		default:	'-',
		global:		true
	},
	modem_type: {
		type:		'select',
		tab:		'general',
		title:		_('Modem type'),
		optional:	false,
		default:	'gsm',
		values: {
			'':		 _('-- Please choose --')
		},
		depends: {
			'device': ['-'],
			'!reverse': true
		},
		validate(section_id, value) {
			if (value == '')
				return _('Please choose device type.');
			return true;
		},
		global:		true
	},
	control_device: {
		type:		'rich_select',
		tab:		'general',
		title:		_('Control device'),
		optional:	false,
		default:	'',
		validate(section_id, value) {
			if (value == '')
				return _('Please choose device.');
			return true;
		}
	},
	ppp_device: {
		type:		'rich_select',
		tab:		'general',
		title:		_('PPP device'),
		optional:	false,
		default:	'',
		validate(section_id, value) {
			if (value == '')
				return _('Please choose device.');
			return true;
		}
	},
	net_device: {
		type:		'rich_select',
		tab:		'general',
		title:		_('Net device'),
		optional:	false,
		default:	'',
		validate(section_id, value) {
			if (value == '')
				return _('Please choose device.');
			return true;
		}
	},
	net_type: {
		type:		'select',
		tab:		'general',
		title:		_('Network type'),
		optional:	false,
		default:	'gsm',
		values: {
			gsm:		_('GSM/UMTS/LTE'),
			cdma:		_('CDMA/EV-DO')
		}
	},
	
	// PDP settings
	pdp_type: {
		type:		'select',
		tab:		'modem',
		title:		_('PDP type'),
		optional:	false,
		default:	'IP',
		values: {
			IP:			_('IP'),
			IPV6:		_('IPV6'),
			IPV4V6:		_('IPV4V6'),
		},
		depends: {
			network_type: ['gsm']
		}
	},
	apn: {
		type:		'text',
		tab:		'modem',
		title:		_('APN'),
		default:	'internet',
		optional:	true,
		validate(section_id, value) {
			if (/^\s+$/.test(value))
				return _('Please choose APN.');
			if (!/^[\w\d._-]+$/i.test(value))
				return _('Invalid APN');
			return true;
		},
		depends: {
			network_type: ['gsm']
		}
	},
	auth_type: {
		type:		'select',
		tab:		'modem',
		title:		_('Auth type'),
		optional:	false,
		default:	'',
		values: {
			'':			_('None'),
			pap:		_('PAP'),
			chap:		_('CHAP'),
		}
	},
	username: {
		type:		'text',
		tab:		'modem',
		title:		_('Username'),
		default:	'',
		optional:	true
	},
	password: {
		type:		'password',
		tab:		'modem',
		title:		_('Password'),
		default:	'',
		optional:	true
	},
	
	// Security
	pin_code: {
		type:		'text',
		tab:		'modem',
		title:		_('PIN code'),
		descr:		_('PIN code for automatic unlocking SIM card.'),
		default:	'',
		optional:	true,
		validate:	'and(uinteger,minlength(4),maxlength(8))'
	},
	mep_code: {
		type:		'text',
		tab:		'modem',
		title:		_('MEP code'),
		descr:		_('MEP code for automatic unlocking Modem (simlock).'),
		default:	'',
		optional:	true
	},
	
	// Misc
	prefer_dhcp: {
		type:		'checkbox',
		tab:		'modem',
		title:		_('Force use DHCP'),
		descr: [
			_('By default, the usbmodem daemon monitors IP address changes and manually sets it to a static interface.'),
			_('This is the fastest way to reconnect to the Internet when the ISP resets the session.'),
			_('But a rollback to DHCP is also available (for worth case).')
		],
		default:	'',
		optional:	true
	}
};

// Customize dropdown
const MyDropdown = ui.Dropdown.extend({
	render() {
		let content = ui.Dropdown.prototype.render.apply(this, arguments);
		content.classList.add('cbi-dropdown-usbmodem');
		this.transformCategories(content);
		
		if (!document.getElementById('usbmodem-dropdown-styles')) {
			document.head.appendChild(E('style', { id: 'usbmodem-dropdown-styles' }, [`
				.cbi-dropdown-usbmodem[open] > ul.dropdown > li[unselectable] {
					opacity: 1;
					border-top-width: 1px;
					border-top-style: solid;
					border-top-color: inherit;
				}
				
				.cbi-dropdown-usbmodem[open] > ul.dropdown > li[unselectable] .hide-close {
					overflow: hidden;
					word-break: break-all;
					max-width: 35em;
				}
			`]));
		}
		
		return content;
	},
	transformCategories(content) {
		content.querySelectorAll('[data-type="category"]').forEach((el) => {
			let li = el.parentNode;
			
			el.removeAttribute('data-type');
			li.classList.add('cbi-dropdown-category');
			li.setAttribute('unselectable', '');
		});
	}
});

// Customize ListValue
const RichListValue = form.ListValue.extend({
	__name__: 'CBI.RichListValue',
	isRendered() {
		return !!this.is_rendered;
	},
	renderWidget(section_id, option_index, cfgvalue) {
		let choices = this.transformChoices() || {};
		let value = (cfgvalue != null) ? cfgvalue : this.default;
		
		this.is_rendered = true;
		
		let widget = new MyDropdown(value, choices, {
			id: this.cbid(section_id),
			sort: this.keylist,
			create: true,
			optional: this.optional || this.rmempty,
			select_placeholder: _('-- Please choose --'),
			custom_placeholder: this.custom_placeholder || this.placeholder,
			validate: L.bind(this.validate, this, section_id),
			disabled: (this.readonly != null) ? this.readonly : this.map.readonly
		});
		return widget.render();
	},
	addCategory(title, descr) {
		this.category_id = this.category_id || 0;
		
		let nodes;
		let value = '#category-' + this.category_id++;
		
		if (descr) {
			nodes = E([], [
				E('div', { 'class': 'hide-close', 'data-type': 'category' }, [
					E('b', [ title ]),
					E('br'),
					E('small', {}, descr)
				])
			]);
		} else {
			nodes = E([], [
				E('div', { 'class': 'hide-close', 'data-type': 'category' }, E('b', {}, title))
			]);
		}
		
		// Openwrt Developers, what a hell???
		if (this.is_rendered) {
			this.getUIElement(this.section.section).addChoices([value], {[value]: nodes});
		} else {
			form.ListValue.prototype.value.call(this, value, nodes);
		}
	},
	value(value, title_open, title_close, description) {
		let nodes;
		if (description) {
			nodes = E([], [
				E('span', { 'class': 'hide-open' }, [ title_open || title_close ]),
				E('div', { 'class': 'hide-close' }, [
					E('b', [ title_close || title_open ]),
					E('br'),
					E('span', {}, description)
				])
			]);
		} else {
			nodes = E([], [
				E('span', { 'class': 'hide-open' }, [ title_open || title_close ]),
				E('div', { 'class': 'hide-close' }, [ title_close || title_open ])
			]);
		}
		
		// Openwrt Developers, what a hell???
		if (this.is_rendered) {
			this.getUIElement(this.section.section).addChoices([value], {[value]: nodes});
		} else {
			form.ListValue.prototype.value.call(this, value, nodes);
		}
	}
});

return network.registerProtocol('usbmodem', {
	getI18n() {
		return _('USB Modem');
	},
	getIfname() {
		return this._ubus('l3_device') || 'usbmodem-%s'.format(this.sid);
	},
	getOpkgPackage() {
		return 'usbmodem';
	},
	isFloating() {
		return true;
	},
	isVirtual() {
		return true;
	},
	getDevices() {
		return null;
	},
	containsDevice(ifname) {
		return (network.getIfnameOf(ifname) == this.getIfname());
	},
	discoverUsb() {
		if (!this.discover_promise) {
			this.discover_promise = fs.exec("/usr/sbin/usbmodem", ["discover-json", this.sid]).then((res) => {
				return JSON.parse(res.stdout.trim());
			});
		}
		return this.discover_promise;
	},
	getUsbDevice(url) {
		return this.discoverUsb().then((response) => {
			for (let usb of response.usb) {
				if (usb.url == url)
					return usb;
			}
			return null;
		});
	},
	renderFormOptions(s) {
		s.tabs["modem"] || s.tab('modem', _('Modem Settings'));
		
		let fields = deepClone(MODEM_FIELDS);
		for (let type in MODEM_TYPES) {
			let type_info = MODEM_TYPES[type];
			
			fields.modem_type.values[type] = type_info.title;
			
			for (let field_name of type_info.fields) {
				if (!fields[field_name]) {
					console.error(`Unknown field: ${field_name}`);
					continue;
				}
				
				if (!fields[field_name].global) {
					fields[field_name].depends ||= {};
					fields[field_name].depends.modem_type ||= [];
					fields[field_name].depends.modem_type.push(type);
				}
			}
		}
		
		fields.device.load = (option, section_id) => {
			return this.discoverUsb().then((response) => {
				option.value('-', _('-- Please choose --'));
				option.value('', _('Custom TTY'));
				
				for (let usb of response.usb) {
					let title = usb.name + (usb.plugged ? "" : _(' (unplugged)'));
					option.value(usb.url, title, title, E('div', {}, [
						'USB: ' + '%04x:%04x'.format(usb.vid, usb.pid),
						'; ',
						'S/N: ' + (usb.serial || '-')
					]));
				}
				
				return [
					response.alias,
					RichListValue.prototype.load.apply(option, [section_id])
				];
			}).then(([alias, value]) => {
				return alias[value] || value;
			});
		};
		
		fields.device.onchange = () => {
			let device = s.getOption('device').formvalue(s.section);
			
			let net_params = ['net_device'];
			let tty_params = ['control_device', 'ppp_device'];
			let reset_params = ['control_device', 'ppp_device', 'net_device', 'modem_type', 'net_type'];
			
			for (let k of reset_params)
				s.getOption(k).getUIElement(s.section).setValue('');
			
			if (device == '') {
				this.discoverUsb().then((response) => {
					for (let k of tty_params)
						updateTTYList(s.getOption(k), response.tty);
					
					for (let k of net_params)
						updateNetList(s.getOption(k), response.net);
				});
			} else {
				this.getUsbDevice(device).then((usb) => {
					if (!usb || !usb.modem)
						return;
					
					for (let k of tty_params)
						updateTTYList(s.getOption(k), usb.tty);
					
					for (let k of net_params)
						updateNetList(s.getOption(k), usb.net);
					
					let modem = usb.modem;
					
					if (modem.type)
						s.getOption('modem_type').getUIElement(s.section).setValue(modem.type);
					
					if (modem.control)
						s.getOption('control_device').getUIElement(s.section).setValue(modem.control);
					
					if (modem.data)
						s.getOption('ppp_device').getUIElement(s.section).setValue(modem.data);
					
					if (modem.net)
						s.getOption('net_device').getUIElement(s.section).setValue(modem.net);
					
					if (modem.net_type)
						s.getOption('net_type').getUIElement(s.section).setValue(modem.net_type);
				});
			}
		};
		
		fields.net_device.load = (option, section_id) => {
			option.value('', _('-- Please choose --'));
			
			let device = uci.get('network', s.section, 'device');
			if (device == '') {
				return this.discoverUsb().then((response) => {
					updateNetList(option, response.net);
					return RichListValue.prototype.load.apply(option, [section_id]);
				});
			} else {
				return this.getUsbDevice(device).then((usb) => {
					usb && updateNetList(option, usb.net);
					return RichListValue.prototype.load.apply(option, [section_id]);
				});
			}
		}
		
		fields.control_device.load = fields.ppp_device.load = (option, section_id) => {
			option.value('', _('-- Please choose --'));
			
			let device = uci.get('network', s.section, 'device');
			if (device == '') {
				return this.discoverUsb().then((response) => {
					updateTTYList(option, response.tty);
					return RichListValue.prototype.load.apply(option, [section_id]);
				});
			} else {
				return this.getUsbDevice(device).then((usb) => {
					usb && updateTTYList(option, usb.tty);
					return RichListValue.prototype.load.apply(option, [section_id]);
				});
			}
		};
		
		for (let field_name in fields)
			createOption(s, field_name, fields[field_name]);
	}
});

function updateTTYList(option, tty_list) {
	if (option.isRendered()) {
		let widget = option.getUIElement(option.section.section);
		widget.setValue('');
		widget.clearChoices();
	}
	
	for (let tty of tty_list) {
		if (typeof tty == 'string') {
			option.value(tty, tty);
		} else if (tty.name) {
			option.value(tty.path, `${tty.path} (${tty.name})`, `${tty.path} (${tty.name}${(tty.title ? ' - ' + tty.title : '')})`);
		} else {
			option.value(tty.path, tty.path);
		}
	}
}

function updateNetList(option, net_list) {
	if (option.isRendered()) {
		let widget = option.getUIElement(option.section.section);
		widget.setValue('');
		widget.clearChoices();
	}
	
	for (let net of net_list) {
		if (typeof tty == 'string') {
			option.value(net, net);
		} else if (net.name) {
			option.value(net.path, `${net.path} (${net.name})`, `${net.path} (${net.name}${(net.title ? ' - ' + net.title : '')})`);
		} else {
			option.value(net.path, net.path);
		}
	}
}

function throttle(callback, time) {
	let flag = false;
	return function () {
		if (flag)
			return;
		flag = true;
		
		let self = this;
		let args = arguments;
		
		setTimeout(() => {
			flag = false;
			callback.apply(self, args);
		}, time || 0);
	}
}

function createOption(s, name, config) {
	let option;
	let descr = Array.isArray(config.descr) ? config.descr.join('<br />') : config.descr;
	
	switch (config.type) {
		case "select":
			option = s.taboption(config.tab, form.ListValue, name, config.title, descr);
		break;
		
		case "rich_select":
			option = s.taboption(config.tab, RichListValue, name, config.title, descr);
		break;
		
		case "checkbox":
			option = s.taboption(config.tab, form.Flag, name, config.title, descr);
		break;
		
		case "text":
			option = s.taboption(config.tab, form.Value, name, config.title, descr);
		break;
		
		case "password":
			option = s.taboption(config.tab, form.Value, name, config.title, descr);
			option.password = true;
		break;
		
		default:
			console.error('Unknown type: ' + config.type);
			return null;
		break;
	}
	
	if (config.optional != null)
		option.optional = config.optional;
	
	if (config.default != null)
		option.default = config.default;
	
	if (config.optional)
		option.rmempty = true;
	
	if (config.onchange)
		option.onchange = config.onchange;
	
	if (config.load) {
		option.load = function () {
			return config.load.apply(this, [this, ...arguments]);
		};
	}
	
	if (config.dummy) {
		option.remove = () => {};
		option.write = () => {};
	} else {
		option.ucioption = name;
	}
	
	if (config.values) {
		for (let k in config.values)
			option.value(k, config.values[k]);
	}
	
	if (config.validate) {
		if (typeof config.validate == 'string') {
			option.datatype = config.validate;
		} else {
			option.validate = config.validate;
		}
	}
	
	let depends = {};
	if (config.depends) {
		for (let k in config.depends) {
			if (k.charAt(0) == '!') {
				depends[k] = config.depends[k];
			} else {
				if (config.depends[k].length > 1) {
					depends[k] = new RegExp('^(' + config.depends[k].join('|') + ')$', 'i');
				} else {
					depends[k] = config.depends[k][0];
				}
			}
		}
	}
	
	if (Object.keys(depends).length)
		option.depends(depends);
}

function deepClone(v) {
	return JSON.parse(JSON.stringify(v));
}
