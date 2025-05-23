[+ autogen5 template +]
[+INCLUDE (string-append "licenses/" (get "License") ".tpl") \+]
[+INCLUDE (string-append "indent.tpl") \+]
/* [+INVOKE EMACS-MODELINE MODE="js2" \+] */
[+INVOKE START-INDENT\+]
/*
 * extension.js
 * Copyright (C) [+(shell "date +%Y")+] [+Author+] <[+Email+]>
 * 
[+INVOKE LICENSE-DESCRIPTION PFX=" * " PROGRAM=(get "Name") OWNER=(get "Author") \+]
 */

const St = imports.gi.St;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

// Other javascript files in the [+UUID+] directory are accesible via Extension.<file name>
const Extension = imports.ui.extensionSystem.extensions['[+UUID+]'];

let text, button;

function _hideHello() {
	Main.uiGroup.remove_actor(text);
	text = null;
}

function _showHello() {
	if (!text) {
		text = new St.Label({ style_class: '[+NameHLower+]-label', text: "Hello, world!" });
		Main.uiGroup.add_actor(text);
	}

	text.opacity = 255;

	let monitor = Main.layoutManager.primaryMonitor;

	text.set_position(Math.floor(monitor.width / 2 - text.width / 2),
		Math.floor(monitor.height / 2 - text.height / 2));

	Tweener.addTween(text,
		{ opacity: 0,
		time: 2,
		transition: 'easeOutQuad',
		onComplete: _hideHello });
}

function init() {
	button = new St.Bin({ style_class: 'panel-button',
	                      reactive: true,
	                      can_focus: true,
	                      x_fill: true,
	                      y_fill: false,
	                      track_hover: true });
	let icon = new St.Icon({ icon_name: 'system-run',
	                         icon_type: St.IconType.SYMBOLIC,
	                         style_class: 'system-status-icon' });

	button.set_child(icon);
	button.connect('button-press-event', _showHello);
}

function enable() {
	Main.panel._rightBox.insert_actor(button, 0);
}

function disable() {
	Main.panel._rightBox.remove_actor(button);
}
[+INVOKE END-INDENT\+]
