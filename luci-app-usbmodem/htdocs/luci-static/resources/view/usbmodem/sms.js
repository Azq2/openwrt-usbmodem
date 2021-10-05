'use strict';
'require fs';
'require view';
'require rpc';
'require ui';
'require form';

var GSM7_CHARS = [
	"\\u000a", "\\u000c-\\u000d", "\\u0020-\\u005f", "\\u0061-\\u007e", "\\u00a0-\\u00a1",
	"\\u00a3-\\u00a5", "\\u00a7", "\\u00bf", "\\u00c4-\\u00c7", "\\u00c9", "\\u00d1", "\\u00d6",
	"\\u00d8", "\\u00dc", "\\u00df-\\u00e0", "\\u00e4-\\u00e6", "\\u00e8-\\u00e9", "\\u00ec",
	"\\u00f1-\\u00f2", "\\u00f6", "\\u00f8-\\u00f9", "\\u00fc", "\\u0393-\\u0394", "\\u0398", "\\u039b",
	"\\u039e", "\\u03a0", "\\u03a3", "\\u03a6", "\\u03a8-\\u03a9", "\\u20ac"
];

var RE_GSM7_CHARS = new RegExp('^[' + GSM7_CHARS.join('') + ']+$', '');

var RE_GSM7_ESC_CHARS = /([\f^{}\\\[~\]\|€])/g;

var SMS_MEMORY_NAMES = {
	SM:		_("SIM Memory"),
	ME:		_("Modem Memory"),
	MT:		_("Modem + SIM Memory")
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

function progressbar(value, max, byte) {
	var vn = parseInt(value) || 0,
	    mn = parseInt(max) || 100,
	    fv = byte ? String.format('%1024.2mB', value) : value,
	    fm = byte ? String.format('%1024.2mB', max) : max,
	    pc = Math.floor((100 / mn) * vn);

	return E('div', {
		'class': 'cbi-progressbar',
		'title': '%s / %s (%d%%)'.format(fv, fm, pc)
	}, E('div', { 'style': 'width:%.2f%%'.format(pc) }));
}

function getSmsCount(value) {
	var capacity, parts;
	
	value = value || "";
	
	if (RE_GSM7_CHARS.test(value) || !value.length) {
		var tmp = value.replace(RE_GSM7_ESC_CHARS, "\x1B$1");
		
		if (tmp.length <= 160) {
			parts = 1;
			capacity = 160 - tmp.length;
		} else {
			parts = 0;
			
			while (tmp.length) {
				capacity = 153 - tmp.length;
				tmp = tmp.substr(tmp[153] === "\x1B" ? 152 : 153);
				parts++;
			}
		}
	} else {
		if (value.length <= 70) {
			parts = 1;
			capacity = 70 - value.length;
		} else {
			parts = Math.ceil(value.length / 67);
			capacity = 67 - (value.length - ((parts - 1) * 67));
		}
	}
	
	return _('%d (%d sms)').format(capacity, parts);
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
		
		if (title) {
			return E('div', {}, [ E('h3', {}, [ title ]), table ]);
		} else {
			return E('div', {}, [ table ]);
		}
	},
	refresh: function () {
		var self = this;
		return self.updateSmsList(self.active_tab, false);
	},
	deletedSelected: function () {
		var self = this;
		console.log(document.querySelectorAll('input[name="sms-select"]'));
	},
	onTabSelected: function (iface, e) {
		var self = this;
		self.active_tab = iface;
		self.active_dir = 'inbox';
		return self.refresh();
	},
	onDirSelected: function (dir, e) {
		var self = this;
		self.active_dir = dir;
		return self.refresh();
	},
	onMessageSelected: function (global, event) {
		var self = this;
		
		var checked_cnt = 0;
		self.view.querySelectorAll('input[name="sms-select"]').forEach(function (el) {
			if (global)
				el.checked = event.target.checked;
			
			if (el.checked)
				checked_cnt++;
		});
		
		document.getElementById('delete-sms').classList.toggle('hidden', !checked_cnt);
	},
	createSms: function (address) {
		var self = this;
		
		var form_data = {
			message: {
				address:	address,
				text:		null
			}
		};
		
		var m = new form.JSONMap(form_data);
		var s = m.section(form.NamedSection, 'message', 'message');
		
		var address_widget = s.option(form.Value, 'address', _('Address'));
		address_widget.placeholder = '+';
		address_widget.validate = function (name, value) {
			var normalized = value.replace(/\s+/, '');
			if (value.length > 0 && !/^\+?([0-9]{1,20})$/.test(normalized)) {
				return [
					_('Wrong number format.'),
					_('Use the format +1234567 for international numbers and 1234567 for local numbers.'),
					_('Maximum number length 20 characters (excluding + and spaces).')
				].join("\n");
			}
			return true;
		};
		
		var text_widget = s.option(form.TextValue, 'text', _('Message text'), getSmsCount(form_data.message.text));
		text_widget.rows = 4;
		text_widget.validate = function (name, value) {
			text_widget.description = getSmsCount(value);
			
			var description_el = document.querySelector('#cbi-json-message-text .cbi-value-description');
			if (description_el)
				description_el.textContent = text_widget.description;
			
			return true;
		};
		
		m.render().then(function (result) {
			ui.showModal(_('New message'), [
				result,
				E('div', { 'class': 'right' }, [
					E('button', {
						'class': 'btn',
						'click': ui.hideModal
					}, _('Cancel')), ' ',
					E('button', {
						'class': 'btn cbi-button cbi-button-positive important',
						'click': ui.createHandlerFn(self, 'onSendSms', form_data)
					}, _('Send'))
				])
			], 'cbi-modal');
		});
	},
	renderSms: function (msg) {
		var self = this;
		
		var text = "";
		var parts = [];
		msg.parts.forEach(function (p) {
			if (p.id >= 0) {
				text += p.text;
				parts.push(p.id);
			} else {
				text += '<...>';
			}
		});
		
		var date = '';
		if (msg.time) {
			var ctime = new Date(msg.time * 1000);
			date = '%04d-%02d-%02d %02d:%02d:%02d'.format(
				ctime.getFullYear(),
				ctime.getMonth() + 1,
				ctime.getDate(),
				ctime.getHours(),
				ctime.getMinutes(),
				ctime.getSeconds()
			);
		}
		
		let address = msg.invalid ? 'DecodeError' : msg.addr;
		
		return E('tr', { 'class': 'tr cbi-section-table-row', 'id': 'sms-' + msg.hash }, [
			E('td', { 'class': 'td cbi-value-field left top' }, [
				E('label', { 'class': 'cbi-checkbox' }, [
					E('input', {
						'type': 'checkbox',
						'name': 'sms-select',
						'value': parts,
						'style': 'vertical-align: text-bottom; margin: 0',
						'click': ui.createHandlerFn(self, 'onMessageSelected', false)
					})
				]),
			]),
			E('td', { 'class': 'td cbi-value-field left top' }, [
				E('a', {
					'class': 'cbi-value-title',
					'href': '#',
					'click': ui.createHandlerFn(self, 'createSms', address)
				}, [
					E('b', { }, [ address ])
				])
			]),
			(
				self.active_dir == 'outbox' || self.active_dir == 'drafts' ?
				'' :
				E('td', { 'class': 'td cbi-value-field left top nowrap' }, [ date ])
			),
			E('td', { 'class': 'td cbi-value-field left top', 'width': '70%' }, [
				E('div', { 'style': 'word-break: break-word; white-space: pre-wrap;' }, [ text ])
			])
		]);
	},
	updateSmsList: function (iface, show_spinner) {
		var self = this;
		
		var sms_list = document.querySelector('#usbmodem-sms-' + iface);
		
		if (show_spinner) {
			sms_list.innerHTML = '';
			sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Loading status...')));
		}
		
		var SMS_DIRS = {
			0:		'inbox',
			1:		'inbox',
			2:		'outbox',
			3:		'drafts'
		};
		
		var SMS_DIR_NAMES = {
			'inbox':	_('Inbox (%d)'),
			'outbox':	_('Outbox (%d)'),
			'drafts':	_('Drafts (%d)'),
		};
		
		return callUsbmodem(self.active_tab, 'read_sms').then(function (result) {
			sms_list.innerHTML = '';
			
			var counters = {
				inbox:	0,
				outbox:	0,
				drafts:	0
			};
			
			var messages_in_dir = [];
			result.messages.reverse().forEach(function (msg) {
				var dir_id = SMS_DIRS[msg.dir];
				counters[dir_id]++;
				
				if (SMS_DIRS[msg.dir] == self.active_dir)
					messages_in_dir.push(msg);
			});
			
			// Capacity info
			var used_sms_pct = result.capacity.used / result.capacity.total * 100;
			var storage_progressbar = E('div', {
				'class': 'cbi-progressbar',
				'title': '%s: %d / %d (%d%%)'.format(SMS_MEMORY_NAMES[result.storage], result.capacity.used, result.capacity.total, used_sms_pct)
			}, E('div', { 'style': 'width:%.2f%%'.format(used_sms_pct) }));
			
			sms_list.appendChild(storage_progressbar);
			
			if (result.capacity.used >= result.capacity.total) {
				sms_list.appendChild(E('div', { 'class': 'alert-message error' }, [
					_('The message memory is full. New messages could not be received.')
				]));
			}
			
			// SMS dirs tabs
			var dirs_tabs = [];
			for (var dir_id in SMS_DIR_NAMES) {
				dirs_tabs.push(E('li', { 'class': self.active_dir == dir_id ? 'cbi-tab' : 'cbi-tab-disabled' }, [
					E('a', {
						'href': '#',
						'click': ui.createHandlerFn(self, 'onDirSelected', dir_id)
					}, [ SMS_DIR_NAMES[dir_id].format(counters[dir_id]) ])
				]));
			}
			sms_list.appendChild(E('ul', { 'class': 'cbi-tabmenu' }, dirs_tabs));
			
			// Actions
			sms_list.appendChild(E('div', { 'class': 'controls', 'style': 'margin: 0 0 18px 0' }, [
				E('button', {
					'class': 'btn cbi-button-add',
					'click': ui.createHandlerFn(self, 'createSms', '')
				}, [ _('New message') ]),
				' ',
				E('button', {
					'class': 'btn cbi-button-neutral',
					'click': ui.createHandlerFn(self, 'refresh')
				}, [ _('Refresh') ]),
				' ',
				E('button', {
					'class': 'btn cbi-button-remove hidden',
					'id': 'delete-sms',
					'click': ui.createHandlerFn(self, 'deletedSelected')
				}, [ _('Delete selected') ])
			]));
			
			// Render messages
			if (messages_in_dir.length > 0) {
				var table = E('table', { 'class': 'table cbi-section-table' }, [
					E('tr', { 'class': 'tr cbi-section-table-titles' }, [
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [
							E('label', { 'class': 'cbi-checkbox' }, [
								E('input', {
									'type': 'checkbox',
									'name': 'global-sms-select',
									'style': 'vertical-align: text-bottom; margin: 0',
									'click': ui.createHandlerFn(self, 'onMessageSelected', true)
								})
							])
						]),
						(
							self.active_dir == 'outbox' || self.active_dir == 'drafts' ?
							'' :
							E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('From') ])
						),
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Date') ]),
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Text') ])
					])
				]);
				
				messages_in_dir.forEach(function (msg) {
					table.appendChild(self.renderSms(msg));
				});
				sms_list.appendChild(table);
			} else {
				sms_list.appendChild(E('p', {}, _('No messages.')));
			}
			
			console.log(result);
		}).catch(function (err) {
			console.error(err);
			
			sms_list.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				sms_list.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
				sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
			} else {
				sms_list.appendChild(E('p', { "class": "alert-message error" }, err.message));
			}
			
			setTimeout(function () {
				self.refresh();
			}, 5000);
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
				'cbi-tab-active': ui.createHandlerFn(self, 'onTabSelected', modem.interface)
			}, [
				E('div', {'id': 'usbmodem-sms-' + modem.interface}, [])
			]));
		});
		
		var view = E('div', {}, [
			E('div', {}, tabs)
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		self.view = view;
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
