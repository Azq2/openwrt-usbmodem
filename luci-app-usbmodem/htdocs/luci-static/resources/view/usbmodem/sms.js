'use strict';
'require usbmodem';
'require fs';
'require view';
'require rpc';
'require ui';
'require form';

let GSM7_CHARS = [
	"\\u000a", "\\u000c-\\u000d", "\\u0020-\\u005f", "\\u0061-\\u007e", "\\u00a0-\\u00a1",
	"\\u00a3-\\u00a5", "\\u00a7", "\\u00bf", "\\u00c4-\\u00c7", "\\u00c9", "\\u00d1", "\\u00d6",
	"\\u00d8", "\\u00dc", "\\u00df-\\u00e0", "\\u00e4-\\u00e6", "\\u00e8-\\u00e9", "\\u00ec",
	"\\u00f1-\\u00f2", "\\u00f6", "\\u00f8-\\u00f9", "\\u00fc", "\\u0393-\\u0394", "\\u0398", "\\u039b",
	"\\u039e", "\\u03a0", "\\u03a3", "\\u03a6", "\\u03a8-\\u03a9", "\\u20ac"
];

let RE_GSM7_CHARS = new RegExp('^[' + GSM7_CHARS.join('') + ']+$', '');

let RE_GSM7_ESC_CHARS = /([\f^{}\\\[~\]\|€])/g;

let SMS_MEMORY_NAMES = {
	SM:		_("SIM Memory"),
	ME:		_("Modem Memory"),
	MT:		_("Modem + SIM Memory")
};

function progressbar(value, max, byte) {
	let vn = parseInt(value) || 0,
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
	let capacity, parts;
	
	value = value || "";
	
	if (RE_GSM7_CHARS.test(value) || !value.length) {
		let tmp = value.replace(RE_GSM7_ESC_CHARS, "\x1B$1");
		
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
	load() {
		return usbmodem.getInterfaces();
	},
	createInfoTable(title, fields) {
		let table = E('table', { 'class': 'table' });
		for (let i = 0; i < fields.length; i += 2) {
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
	refresh() {
		return this.updateSmsList(this.active_tab, false);
	},
	deletedSelected() {
		console.log(document.querySelectorAll('input[name="sms-select"]'));
	},
	onTabSelected(iface, e) {
		this.active_tab = iface;
		this.active_dir = 'inbox';
		return this.refresh();
	},
	onDirSelected(dir, e) {
		this.active_dir = dir;
		return this.refresh();
	},
	onMessageSelected(global, event) {
		let checked_cnt = 0;
		this.view.querySelectorAll('input[name="sms-select"]').forEach((el) => {
			if (global)
				el.checked = event.target.checked;
			
			if (el.checked)
				checked_cnt++;
		});
		
		document.getElementById('delete-sms').classList.toggle('hidden', !checked_cnt);
	},
	createSms(address) {
		let form_data = {
			message: {
				address:	address,
				text:		null
			}
		};
		
		let m = new form.JSONMap(form_data);
		let s = m.section(form.NamedSection, 'message', 'message');
		
		let address_widget = s.option(form.Value, 'address', _('Address'));
		address_widget.placeholder = '+';
		address_widget.validate = (name, value) => {
			let normalized = value.replace(/\s+/, '');
			if (value.length > 0 && !/^\+?([0-9]{1,20})$/.test(normalized)) {
				return [
					_('Wrong number format.'),
					_('Use the format +1234567 for international numbers and 1234567 for local numbers.'),
					_('Maximum number length 20 characters (excluding + and spaces).')
				].join("\n");
			}
			return true;
		};
		
		let text_widget = s.option(form.TextValue, 'text', _('Message text'), getSmsCount(form_data.message.text));
		text_widget.rows = 4;
		text_widget.validate = (name, value) => {
			text_widget.description = getSmsCount(value);
			
			let description_el = document.querySelector('#cbi-json-message-text .cbi-value-description');
			if (description_el)
				description_el.textContent = text_widget.description;
			
			return true;
		};
		
		m.render().then((result) => {
			ui.showModal(_('New message'), [
				result,
				E('div', { 'class': 'right' }, [
					E('button', {
						'class': 'btn',
						'click': ui.hideModal
					}, _('Cancel')), ' ',
					E('button', {
						'class': 'btn cbi-button cbi-button-positive important',
						'click': ui.createHandlerFn(this, 'onSendSms', form_data)
					}, _('Send'))
				])
			], 'cbi-modal');
		});
	},
	renderSms(msg) {
		let text = "";
		let parts = [];
		msg.parts.forEach((p) => {
			if (p.id >= 0) {
				text += p.text;
				parts.push(p.id);
			} else {
				text += '<...>';
			}
		});
		
		let date = '';
		if (msg.time) {
			let ctime = new Date(msg.time * 1000);
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
						'click': ui.createHandlerFn(this, 'onMessageSelected', false)
					})
				]),
			]),
			E('td', { 'class': 'td cbi-value-field left top' }, [
				E('a', {
					'class': 'cbi-value-title',
					'href': '#',
					'click': ui.createHandlerFn(this, 'createSms', address)
				}, [
					E('b', { }, [ address ])
				])
			]),
			(
				this.active_dir == 'outbox' || this.active_dir == 'drafts' ?
				'' :
				E('td', { 'class': 'td cbi-value-field left top nowrap' }, [ date ])
			),
			E('td', { 'class': 'td cbi-value-field left top', 'width': '70%' }, [
				E('div', { 'style': 'word-break: break-word; white-space: pre-wrap;' }, [ text ])
			])
		]);
	},
	updateSmsList(iface, show_spinner) {
		let sms_list = document.querySelector('#usbmodem-sms-' + iface);
		
		if (show_spinner) {
			sms_list.innerHTML = '';
			sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Loading status...')));
		}
		
		let SMS_DIRS = {
			0:		'inbox',
			1:		'inbox',
			2:		'outbox',
			3:		'drafts'
		};
		
		let SMS_DIR_NAMES = {
			'inbox':	_('Inbox (%d)'),
			'outbox':	_('Outbox (%d)'),
			'drafts':	_('Drafts (%d)'),
		};
		
		return usbmodem.call(this.active_tab, 'read_sms').then((result) => {
			sms_list.innerHTML = '';
			
			let counters = {
				inbox:	0,
				outbox:	0,
				drafts:	0
			};
			
			let messages_in_dir = [];
			result.messages.reverse().forEach((msg) => {
				let dir_id = SMS_DIRS[msg.dir];
				counters[dir_id]++;
				
				if (SMS_DIRS[msg.dir] == this.active_dir)
					messages_in_dir.push(msg);
			});
			
			// Capacity info
			let used_sms_pct = result.capacity.used / result.capacity.total * 100;
			let storage_progressbar = E('div', {
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
			let dirs_tabs = [];
			for (let dir_id in SMS_DIR_NAMES) {
				dirs_tabs.push(E('li', { 'class': this.active_dir == dir_id ? 'cbi-tab' : 'cbi-tab-disabled' }, [
					E('a', {
						'href': '#',
						'click': ui.createHandlerFn(this, 'onDirSelected', dir_id)
					}, [ SMS_DIR_NAMES[dir_id].format(counters[dir_id]) ])
				]));
			}
			sms_list.appendChild(E('ul', { 'class': 'cbi-tabmenu' }, dirs_tabs));
			
			// Actions
			sms_list.appendChild(E('div', { 'class': 'controls', 'style': 'margin: 0 0 18px 0' }, [
				E('button', {
					'class': 'btn cbi-button-add',
					'click': ui.createHandlerFn(this, 'createSms', '')
				}, [ _('New message') ]),
				' ',
				E('button', {
					'class': 'btn cbi-button-neutral',
					'click': ui.createHandlerFn(this, 'refresh')
				}, [ _('Refresh') ]),
				' ',
				E('button', {
					'class': 'btn cbi-button-remove hidden',
					'id': 'delete-sms',
					'click': ui.createHandlerFn(this, 'deletedSelected')
				}, [ _('Delete selected') ])
			]));
			
			// Render messages
			if (messages_in_dir.length > 0) {
				let table = E('table', { 'class': 'table cbi-section-table' }, [
					E('tr', { 'class': 'tr cbi-section-table-titles' }, [
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [
							E('label', { 'class': 'cbi-checkbox' }, [
								E('input', {
									'type': 'checkbox',
									'name': 'global-sms-select',
									'style': 'vertical-align: text-bottom; margin: 0',
									'click': ui.createHandlerFn(this, 'onMessageSelected', true)
								})
							])
						]),
						(
							this.active_dir == 'outbox' || this.active_dir == 'drafts' ?
							'' :
							E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('From') ])
						),
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Date') ]),
						E('th', { 'class': 'th cbi-section-table-cell left top' }, [ _('Text') ])
					])
				]);
				
				messages_in_dir.forEach((msg) => {
					table.appendChild(this.renderSms(msg));
				});
				sms_list.appendChild(table);
			} else {
				sms_list.appendChild(E('p', {}, _('No messages.')));
			}
			
			console.log(result);
		}).catch((err) => {
			console.error(err);
			
			sms_list.innerHTML = '';
			if (err.message.indexOf('Object not found') >= 0) {
				sms_list.appendChild(E('p', {}, _('Modem not found. Please insert your modem to USB.')));
				sms_list.appendChild(E('p', { 'class': 'spinning' }, _('Waiting for modem...')));
			} else {
				sms_list.appendChild(E('p', { "class": "alert-message error" }, err.message));
			}
			
			setTimeout(() => {
				this.refresh();
			}, 5000);
		});
	},
	render(interfaces) {
		let view = E('div', {}, [
			usbmodem.createModemTabs(interfaces, [this, 'onTabSelected'], (iface) => {
				return E('div', {'id': 'usbmodem-sms-' + iface}, []);
			})
		]);
		
		ui.tabs.initTabGroup(view.lastElementChild.childNodes);
		
		this.view = view;
		
		return view;
	},

	handleSaveApply: null,
	handleSave: null,
	handleReset: null
});
