'use strict';
'require rpc';
'require uci';
'require form';
'require network';
'require fs';
'require ui';

network.registerPatternVirtual(/^usbmodem-.+$/);

const MODEM_TYPES = {
	ppp: {
		title:	_('Classic modem (PPP)'),
		fields:	[
			"modem_device",
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
			"modem_device",
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
			"modem_device",
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
	modem: {
		type:		'rich_select',
		tab:		'general',
		title:		_('Modem'),
		descr:		_('This list contains plugged and detected modems. Choose one for automatic setup.'),
		optional:	false,
		default:	'',
		global:		true,
		dummy:		true
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
		validate(section_id, value) {
			if (value == '')
				return _('Please choose device type.');
			return true;
		},
		global:		true
	},
	modem_device: {
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
	},
	createChoiceElement(sb, value, label) {
		console.log('createChoiceElement');
		return ui.Dropdown.prototype.createChoiceElement.apply(this, arguments);
	}
});

// Customize ListValue
const RichListValue = form.ListValue.extend({
	__name__: 'CBI.RichListValue',
	renderWidget(section_id, option_index, cfgvalue) {
		let choices = this.transformChoices() || {};
		let value = (cfgvalue != null) ? cfgvalue : this.default;
		
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
		
		if (descr) {
			form.ListValue.prototype.value.call(this, '#category-' + this.category_id++, E([], [
				E('div', { 'class': 'hide-close', 'data-type': 'category' }, [
					E('b', [ title ]),
					E('br'),
					E('small', {}, descr)
				])
			]));
		} else {
			form.ListValue.prototype.value.call(this, '#category-' + this.category_id++, E([], [
				E('div', { 'class': 'hide-close', 'data-type': 'category' }, E('b', {}, title))
			]));
		}
	},
	value(value, title_open, title_close, description) {
		if (description) {
			form.ListValue.prototype.value.call(this, value, E([], [
				E('span', { 'class': 'hide-open' }, [ title_open || title_close ]),
				E('div', { 'class': 'hide-close' }, [
					E('b', [ title_close || title_open ]),
					E('br'),
					E('span', {}, description)
				])
			]));
		} else {
			form.ListValue.prototype.value.call(this, value, E([], [
				E('span', { 'class': 'hide-open' }, [ title_open || title_close ]),
				E('div', { 'class': 'hide-close' }, [ title_close || title_open ])
			]));
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
		
		let ignore_reset = false;
		
		let select_modem = throttle(() => {
			ignore_reset = true;
			
			let value = s.getOption('modem').getUIElement(s.section).getValue();
			if (value != '') {
				let json = JSON.parse(value);
				
				if (json.type)
					s.getOption('modem_type').getUIElement(s.section).setValue(json.type);
				
				if (json.control)
					s.getOption('modem_device').getUIElement(s.section).setValue(json.control);
				
				if (json.data)
					s.getOption('ppp_device').getUIElement(s.section).setValue(json.data);
				
				if (json.net_type)
					s.getOption('net_type').getUIElement(s.section).setValue(json.net_type);
				
				if (json.net)
					s.getOption('net_device').getUIElement(s.section).setValue(json.net);
			}
			
			setTimeout(() => {
				ignore_reset = false;
			}, 0);
		});
		
		let reset_modem = throttle(() => {
			if (!ignore_reset)
				s.getOption('modem').getUIElement(s.section).setValue('');
		});
		
		fields.modem.load = loadModems;
		fields.modem.onchange = select_modem;
		
		fields.modem_device.load = loadTTYDevices;
		fields.modem_device.onchange = reset_modem;
		
		fields.ppp_device.load = loadTTYDevices;
		fields.ppp_device.onchange = reset_modem;
		
		fields.net_device.load = loadNetDevices;
		fields.net_device.onchange = reset_modem;
		
		fields.net_type.onchange = reset_modem;
		
		for (let field_name in fields)
			createOption(s, field_name, fields[field_name]);
	}
});

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

function loadModems() {
	return fs.exec("/usr/sbin/usbmodem", ["discover", "modems"]).then((res) => {
		let response = JSON.parse(res.stdout.trim());
		
		this.value('', _('Custom'));
		
		for (let modem of response.modems) {
			this.value(JSON.stringify(modem), modem.name, modem.name, E('div', {}, [
				'USB: ' + '%04x:%04x'.format(modem.vid, modem.pid),
				'; ',
				'S/N: ' + (modem.serial || '-')
			]));
		}
	});
}

function loadNetDevices(section_id) {
	return Promise.all([
		fs.exec("/usr/sbin/usbmodem", ["discover", "modems"]),
		getNetworkDevices()
	]).then(([res, interfaces]) => {
		let response = JSON.parse(res.stdout.trim());
		for (let dev of response.usb) {
			if (dev.net.length > 0) {
				this.addCategory(dev.name, '<b>ID:</b> %04x:%04x; <b>S/N:</b> %s'.format(dev.vid, dev.pid, dev.serial || '-'));
				for (let net of dev.net)
					this.value(net.url, net.url, net.title ? `${net.title} (${net.name})` : net.name);
			}
		}
		
		if (interfaces.length > 0) {
			this.addCategory(_('Other ethernets'));
			for (let iface of interfaces)
				this.value(iface, iface);
		}
		
		return RichListValue.prototype.load.apply(this, [section_id]);
	});
}

function loadTTYDevices(section_id) {
	return fs.exec("/usr/sbin/usbmodem", ["discover", "modems"]).then((res) => {
		let response = JSON.parse(res.stdout.trim());
		for (let dev of response.usb) {
			this.addCategory(dev.name, '<b>ID:</b> %04x:%04x; <b>S/N:</b> %s'.format(dev.vid, dev.pid, dev.serial || '-'));
			for (let tty of dev.tty)
				this.value(tty.url, tty.url, tty.title ? `${tty.title} (${tty.name})` : tty.name);
		}
		
		if (response.tty.length > 0) {
			this.addCategory(_('Other TTY\'s'));
			
			for (let tty of response.tty)
				this.value(tty, tty);
		}
		
		return RichListValue.prototype.load.apply(this, [section_id]);
	});
}

function getNetworkDevices() {
	let callback = rpc.declare({
		object: 'network.device',
		method: 'status'
	});
	return callback().then((result) => {
		let interfaces = [];
		for (let iface in result) {
			if (result[iface].devtype == "ethernet")
				interfaces.push(iface);
		}
		return interfaces.sort();
	});
}

function createOption(s, name, field) {
	let widget;
	let descr = Array.isArray(field.descr) ? field.descr.join('<br />') : field.descr;
	
	switch (field.type) {
		case "select":
			widget = s.taboption(field.tab, form.ListValue, name, field.title, descr);
		break;
		
		case "rich_select":
			widget = s.taboption(field.tab, RichListValue, name, field.title, descr);
		break;
		
		case "checkbox":
			widget = s.taboption(field.tab, form.Flag, name, field.title, descr);
		break;
		
		case "text":
			widget = s.taboption(field.tab, form.Value, name, field.title, descr);
		break;
		
		case "password":
			widget = s.taboption(field.tab, form.Value, name, field.title, descr);
			widget.password = true;
		break;
		
		default:
			console.error('Unknown type: ' + field.type);
			return null;
		break;
	}
	
	if (field.optional != null)
		widget.optional = field.optional;
	
	if (field.default != null)
		widget.default = field.default;
	
	if (field.optional)
		widget.rmempty = true;
	
	if (field.onchange)
		widget.onchange = field.onchange;
	
	if (field.load)
		widget.load = field.load;
	
	if (field.dummy) {
		widget.remove = () => {};
		widget.write = () => {};
	} else {
		widget.ucioption = name;
	}
	
	if (field.values) {
		for (let k in field.values)
			widget.value(k, field.values[k]);
	}
	
	if (field.validate) {
		if (typeof field.validate == 'string') {
			widget.datatype = field.validate;
		} else {
			widget.validate = field.validate;
		}
	}
	
	let depends = {};
	if (field.depends) {
		for (let k in field.depends) {
			if (field.depends[k].length > 1) {
				depends[k] = new RegExp('^(' + field.depends[k].join('|') + ')$', 'i');
			} else {
				depends[k] = field.depends[k][0];
			}
		}
	}
	
	if (Object.keys(depends).length)
		widget.depends(depends);
}

function deepClone(v) {
	return JSON.parse(JSON.stringify(v));
}
