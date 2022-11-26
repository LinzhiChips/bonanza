/*
 * Copyright (C) 2022 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

const base_url = window.location.protocol + "//" + window.location.host;


const UPDATE_INTERVAL_S = 10;

var selected_id = null;
var miner_cache = {};
var file_path = {
	active:	null,
	test:	null,
};

/* ----- Helper functions -------------------------------------------------- */


function remove_all_children(e)
{
	// https://stackoverflow.com/a/3955238 (2A)
	while (e.firstChild) {
		e.removeChild(e.lastChild);
	}
}


function pad2(n)
{
	return n < 10 ? "0" + n : n;
}


function format_duration(t, seconds = true)
{
	/* https://stackoverflow.com/a/6313032 */

	var date = new Date(t * 1000);
	var h = date.getUTCHours();
	var m = date.getUTCMinutes();
	var s = date.getSeconds();
	var days = Math.trunc(t / 24 / 3600);

	return (days ? days + "d" : "") +
	    pad2(h) + ":" + pad2(m) +
	    (seconds ? ":" + pad2(s) : "");
}


function format_utc(t, seconds = true)
{
	var d = t == null ? new Date() : new Date(t * 1000);

	if (t == null) {
		t = d.getTime() / 1000;
	}
	return d.getUTCFullYear() + "-" +
	    pad2(d.getUTCMonth() + 1) + "-" +
	    pad2(d.getUTCDate()) + " " +
	    format_duration(t % (24 * 3600), seconds) + "Z";
}


function set_text(id, s)
{
	var e = document.getElementById(id);

	if (s == null) {
		e.innerHTML = "&mdash;";
	} else {
		e.textContent = s;
	}
}


function set_a(id, ipv4)
{
	var e = document.getElementById(id);
	var span = document.createElement("SPAN");

	remove_all_children(e);
	if (ipv4 == null) {
		span.innerHTML = "&mdash;"
		e.appendChild(span);
	} else {
		var a = document.createElement("A");

		a.href = "http://" + ipv4;
		span.textContent = ipv4;
		a.appendChild(span);
		e.appendChild(a);
	}
}


function class_toggle(id, name, on)
{
	var e = document.getElementById(id);

	e.className = e.className.replace(" " + name, "");
	if (on) {
		e.className += " " + name;
	}
}


/* ----- POST -------------------------------------------------------------- */


function submit_post(url, body)
{
	var http = new XMLHttpRequest();

	http.open("POST", url, true);
	http.setRequestHeader("Content-type",
	    "application/x-www-form-urlencoded");
	http.send(body);
	/* @@@ add error handling ?
	http.onload = function() {
		alert(http.responseText);
	}
	*/
}


function start_update(restart)
{
	var button = document.getElementById("update-button");
	var scope = document.getElementById("update-scope").value;
	var params;

	if (selected_id == null && scope != "all") {
		return;
	}

	if (scope == "all") {
		params = "all";
	} else if (scope == "group") {
		var delta_hash = miner_cache[selected_id]["delta_hash"];

		params = "group=" + delta_hash;
	} else {
		params = "id=" + selected_id;
	}

	if (restart) {
		params += "&restart";
	}

	button.disabled = true;
	button = document.getElementById("update-restart-button");
	button.disabled = true;
	submit_post(base_url + "/update", params);
}


/* fetch API */


const do_post = async (url, body, json) => {
	try {
		const response = await fetch(url, {
			method: "POST",
			headers: {
				"Content-Type":
				    "application/x-www-form-urlencoded"
			},
			body: body
		});

		if (!response.ok) {
			throw new Error(response.statusText);
		}

		const data = json ? await response.json() :
		    await response.text();

		return data;
	} catch (err)  {
		return err;
	}
}


/* ----- Miners list ------------------------------------------------------- */


function select_miner(id)
{
	var view = document.getElementById("miner-view");
	var slider = document.getElementById("slider");
	var button = document.getElementById("test-button");

	selected_id = id;
	view.style.display = "block";
	slider.style.display = "flex";
	button.disabled = false;

	var table = document.getElementById("miner");

	remove_all_children(table);

	var tr = table.insertRow();
	var td = tr.insertCell();

	td.textContent = "Retrieving variables ...";

	stop_timer();
	poll_update();
}


function deselect_miner()
{
	var view = document.getElementById("miner-view");
	var slider = document.getElementById("slider");
	var button = document.getElementById("test-button");

	selected_id = null;
	view.style.display = "hidden";
	slider.style.display = "hidden";
	button.disabled = true;
	set_text("miner-serial", null);
}


function identifier(miner)
{
	var span = document.createElement("SPAN");
	var name = miner["name"];
	var ipv4 = miner["ipv4"];
	var id = miner["id"];

	if (name != null && name != "") {
		span.textContent = name;
	} else if (ipv4 != null) {
		span.textContent = ipv4;
	} else {
		span.textContent = "0x" + id.toString(16);
	}
	span.addEventListener("click", function() { select_miner(id); });
	return span;
}


function label(miner)
{
	var name = miner["name"];
	var ipv4 = miner["ipv4"];
	var id = miner["id"];

	if (name != null && name != "") {
		return name;
	}
	if (ipv4 != null) {
		return ipv4;
	}
	return "0x" + id.toString(16);
}


function show_in_outer(s)
{
	var outer = document.getElementById("miners");

	if (outer.textContent != s) {
		remove_all_children(outer);
		outer.textContent = s;
	}
}


function compare(a, b)
{
	return a < b ? -1 : a > b ? 1 : 0;
}


function sort_id_by_label(ids)
{
	return ids.sort((a, b) =>
	    compare(label(miner_cache[a]), label(miner_cache[b])));
}


function sorted_ids(obj)
{
	if (obj instanceof Array) {
		return sort_id_by_label(obj);
	} else {
		return sort_id_by_label(Object.keys(obj));
	}
}


function explain(div, s)
{
	var e = document.createElement("DIV");

	e.className = "explain";
	e.textContent = s;
	div.appendChild(e);
}


function show_miners(data)
{
	var outer = document.getElementById("miners");
	var incomplete_cluster = [];	// we're establishing contact
	var config_cluster = {};	// we have the miner's configuration
	var error_cluster = {};		// well, something went wrong
	var delta_cluster = {};		// we ran the rules and have a delta
	var updating_cluster = [];	// configuration update in progress
	var same_cluster = [];		// delta is empty (no changes needed)
	var restart_cluster = [];	// no delta, restart required

	miner_cache = {};
	if (data instanceof Error) {
		show_in_outer(data.message);
		console.log("data =", data, "(done)");
		return false;
	}
	if (data.length == 0) {
		show_in_outer("Waiting for crew data ...");
		return false;
	}
	remove_all_children(outer);

	/* --- process data and group miners --- */

	for (var m of data) {
		var id = m["id"];
		var config = m["miner_hash"];
		var delta = m["delta_hash"];
		var error = m["error"];
		var state = m["state"];
		var restart = m["restart"];

		miner_cache[id] = m;
		if (error != null) {
			if (!(error in error_cluster)) {
				error_cluster[error] = [];
			}
			error_cluster[error].push(id);
		} else if (delta != null) {
			if (state == "updating") {
				updating_cluster.push(id);
			} else if (state == "same") {
				if (restart == null) {
					same_cluster.push(id);
				} else {
					restart_cluster.push(id);
				}
			} else {
				if (!(delta in delta_cluster)) {
					delta_cluster[delta] = [];
				}
				delta_cluster[delta].push(id);
			}
		} else if (config != null) {
			if (!(config in config_cluster)) {
				config_cluster[config] = [];
			}
			config_cluster[config].push(id);
		} else {
			incomplete_cluster.push(id);
		}
	}

	/* --- sort config and delta clusters alphabetically --- */

	var config_sort = {};
	var delta_sort = {};

	for (var hash of Object.keys(config_cluster)) {
		config_sort[sorted_ids(config_cluster[hash])[0]] = hash;
	}

	for (var hash of Object.keys(delta_cluster)) {
		delta_sort[sorted_ids(delta_cluster[hash])[0]] = hash;
	}

	/* --- show the groups --- */

	var explained = false;

	for (var error of Object.keys(error_cluster).sort()) {
		var div = document.createElement("DIV");

		div.className = "error";
		outer.appendChild(div);

		for (var id of sorted_ids(error_cluster[error])) {
			div.appendChild(identifier(miner_cache[id]));
		}
		if (!explained) {
			explain(div, "Script failed");
			explained = true;
		}
	}

	if (updating_cluster.length > 0) {
		var div = document.createElement("DIV");

		div.className = "updating";
		outer.appendChild(div);

		for (var id of sorted_ids(updating_cluster)) {
			div.appendChild(identifier(miner_cache[id]));
		}
		explain(div, "Updating the miner configuration");
	}

	if (incomplete_cluster.length > 0) {
		var div = document.createElement("DIV");

		div.className = "incomplete";
		outer.appendChild(div);

		for (var id of sorted_ids(incomplete_cluster)) {
			div.appendChild(identifier(miner_cache[id]));
		}
		explain(div, "Waiting for data");
	}

	explained = false;

	for (var group of sorted_ids(config_sort)) {
		var hash = config_sort[group];
		var div = document.createElement("DIV");

		div.className = "config";
		outer.appendChild(div);

		for (var id of sorted_ids(config_cluster[hash])) {
			div.appendChild(identifier(miner_cache[id]));
		}
		if (!explained) {
			explain(div,
			    "Waiting for crew data (have configuration)");
			explained = true;
		}
	}

	explained = false;

	for (var group of sorted_ids(delta_sort)) {
		var hash = delta_sort[group];
		var div = document.createElement("DIV");
		var id = delta_cluster[hash][0];

		div.className = miner_cache[id]["state"];
		outer.appendChild(div);

		for (id of sorted_ids(delta_cluster[hash])) {
			div.appendChild(identifier(miner_cache[id]));
		}
		if (!explained) {
			explain(div, "Configuration differs");
			explained = true;
		}
	}

	if (restart_cluster.length > 0) {
		var div = document.createElement("DIV");

		div.className = "restart";
		outer.appendChild(div);

		for (var id of sorted_ids(restart_cluster)) {
			div.appendChild(identifier(miner_cache[id]));
		}
		explain(div, "Restart required");
	}

	if (same_cluster.length > 0) {
		var div = document.createElement("DIV");

		div.className = "same";
		outer.appendChild(div);

		for (var id of sorted_ids(same_cluster)) {
			div.appendChild(identifier(miner_cache[id]));
		}
		explain(div, "No change needed");
	}

	/* --- update the selected miner, if any --- */

	var m = miner_cache[selected_id];
	var button = document.getElementById("update-button");
	var restart_button = document.getElementById("update-restart-button");

	button.disabled = true;
	restart_button.disabled = true;
	if (m != null) {
		var restart_text =
		    document.getElementById("update-restart-text");
		var ipv4 = m["ipv4"];
		var state;

		if (m["error"] != null) {
			state = "Error";
		} else if (m["delta_hash"] != null) {
			if (m["state"] == "updating") {
				state = "Updating";
			} else {
				state = "Ready";
				if (m["state"] == "same") {
					if (m["restart"]) {
						restart_button.disabled = false;
						restart_text.textContent =
						    "Restart";
					}
				} else {
					button.disabled = false;
					restart_button.disabled = false;
					restart_text.textContent =
					    "Update & restart";
				}
			}
		} else {
			state = "Incomplete";
		}
		set_text("miner-state", state);
		set_text("miner-name", m["name"]);
		set_text("miner-ip", ipv4 == null ? null : ipv4);
		set_a("miner-ip", ipv4);
		set_text("miner-id", "0x" +  m["id"].toString(16));
		set_text("miner-last-seen", format_utc(m["last_seen"]));
	}

	return true;
}


/* ----- Viewer (for the rules file) --------------------------------------- */


/*
 * Derived from fw/www/editor.m4
 * whhich is in turn based on
 * https://www.w3schools.com/howto/howto_css_modals.asp
 */


class Viewer {
	close()
	{
		this.modal.style.display = "none";
	}

	open(text, description = null) {
		this.ta.value = text;
		if (description == null) {
			this.descr.style.display = "none";
		} else {
			this.descr_text.textContent = description;
			this.descr.style.display = "block";
		}
		this.modal.style.display = "block";
	}

	constructor() {
		let self = this;

		this.modal = document.getElementById("viewer");
		this.ta = document.getElementById("viewer-text");
		this.button = document.getElementById("viewer-close");
		this.descr = document.getElementById("viewer-descr");
		this.descr_text = document.getElementById("viewer-descr-text");

		this.button.addEventListener("click",
		    function() { self.close(); });
	}
}


var viewer = new Viewer();


window.onclick = function(ev) {
	if (ev.target == viewer.modal) {
		viewer.close();
	}
};


/* ----- Show miner -------------------------------------------------------- */


function add_delta(table, delta)
{
	for (var d of delta) {
		var name = d["name"];
		var old_value = d["old"];
		var new_value = d["new"];

		if (old_value != null) {
			var tr = table.insertRow();
			var td = tr.insertCell();

			td.textContent = name;
			td = tr.insertCell();
			td.textContent = old_value;
			if (old_value != new_value) {
				tr.className = "del";
			}
		}
		if (new_value != null && old_value != new_value) {
			var tr = table.insertRow();
			var td = tr.insertCell();

			if (old_value == null) {
				td.textContent = name;
			}
			td = tr.insertCell();
			td.textContent = new_value;
			tr.className = "add";
		}
	}
}


function show_miner(data)
{
	var table = document.getElementById("miner");
	var m = miner_cache[selected_id];
	var serial = data["serial"];

	set_text("miner-serial",
	    serial[0] == null && serial[1] == null ? null :
	    (serial[0] == "" ? "-" : serial[0]) + "/" +
	    (serial[1] == "" ? "-" : serial[1]));

	remove_all_children(table);
	if (m != null && m["error"] != null) {
		var tr = table.insertRow();
		var td = tr.insertCell();

		tr.className = "error";
		td.textContent = m["error"];
		td.colSpan = 2;
	}
	if (data instanceof Error) {
		var tr = table.insertRow();
		var td = tr.insertCell();

		td.textContent = data.message;
		return false;
	}
	add_delta(table, data["delta"]);

	return true;
}


/* ----- Slider ------------------------------------------------------------ */


var moving_slider = false;


function slider_end(event) {
	var e = document.getElementById("slider");

	moving_slider = false;
	document.body.removeEventListener("mouseup", slider_end)
	e.removeEventListener("mousemove", slider_mouse_move)
}


function slider_mouse_down(event) {
	moving_slider = true;
	document.body.addEventListener("mousemove", slider_mouse_move)
	document.body.addEventListener("mouseup", slider_end)
}


function slider_mouse_move(event) {
	var e = document.getElementById("miners-view");

	if (moving_slider) {
		e.style.flexBasis = event.clientX + "px";
	} else {
		slider_end(event);
	}
}


{
	var e = document.getElementById("slider");

	e.addEventListener("mousedown", slider_mouse_down)
}


/* ----- Get URL as text or JSON ------------------------------------------- */


// https://stackoverflow.com/a/59916857

const get_text = async url => {
	try {
		const response = await fetch(url);

		if (!response.ok) {
			throw new Error(response.statusText)
		}

		const data = await response.text();

		return data;
	} catch (err) {
		return err;
	}
}

const get_json = async url => {
	try {
		const response = await fetch(url);

		if (!response.ok) {
			throw new Error(response.statusText)
		}

		const data = await response.json();

		return data;
	} catch (err) {
		return err;
	}
}


/* ----- Show rules file --------------------------------------------------- */


function show_rules(path)
{
	var url = base_url + "/" + path.split("/").slice(-2).join("/");

	get_text(url).then(data => {
		viewer.open(data);
	}).catch(error => {
		console.error(error);
	});
}


/* ----- Update path names ------------------------------------------------- */


function set_path(type, path)
{
	var div = document.getElementById("path-" + type);

	div.textContent = path;
	file_path[type] = path;
}


/* ----- Update miners list ------------------------------------------------ */


var timer = null;
var auto_start_timer = false; /* only auto-update once, at start */


function start_timer()
{
	timer = setTimeout(poll_update, UPDATE_INTERVAL_S * 1000)
}


function stop_timer()
{
	clearTimeout(timer);
}


function poll_update()
{
	var miners_url = base_url + "/miners";

	if (timer != null) {
		clearTimeout(timer);
		timer = null;
	}
	get_json(miners_url).then(data => {
		var updated = show_miners(data);

		if (selected_id != null && !(selected_id in miner_cache)) {
			deselect_miner();
		}
		if (selected_id != null) {
			var miner_url = base_url + "/miner?id=0x" +
			    selected_id.toString(16);

			get_json(miner_url).then(data => {
				updated = show_miner(data);
			}).catch(error => {
				console.error(error);
			});
		}
		if (updated) {
			var e = document.getElementById("last-refresh");

			e.textContent = format_utc(null, false);
		}
		if (file_path["active"] == null) {
			get_text(base_url + "/path?type=active").then(data => {
				set_path("active", data);
			}).catch(error => {
				console.error(error);
			});
		}
		if (file_path["test"] == null) {
			get_text(base_url + "/path?type=test").then(data => {
				set_path("test", data);
			}).catch(error => {
				console.error(error);
			});
		}
		if (auto_start_timer) {
			start_timer();
		}
	}).catch(error => {
		console.error(error);
		if (auto_start_timer) {
			start_timer();
		}
	});
}


function auto_refresh(checkbox)
{
	if (checkbox.checked) {
		auto_start_timer = true;
		poll_update();
	} else {
		auto_start_timer = false;
		stop_timer();
	}
}


/* ----- Active reload ----------------------------------------------------- */


function active_reload()
{
	var button = document.getElementById("reload-button");

	button.disabled = true;
	do_post(base_url + "/reload", "", false).then(data => {
		if (data != "") {
			alert(data);
		}
		button.disabled = false;
	}).catch (error => {
		alert(error);
		button.disabled = false;
	});
}


/* ----- Test run ---------------------------------------------------------- */


function test_error(table, error)
{
	var tr = table.insertRow();
	var td = tr.insertCell();

	tr.className = "error";
	td.textContent = error
	td.colSpan = 2;
}


function test_run()
{
	var table = document.getElementById("result");

	if (selected_id == null) {
		return;
	}

	remove_all_children(table);
	do_post(base_url + "/run", "id=" + selected_id, true).then(data => {
		console.log("DATA", data);
		if (data["error"] == null) {
			add_delta(table, data["delta"]);
		} else {
			test_error(table, data["error"]);
		}
	}).catch (error => {
		console.error("ERROR", error);
		test_error(table, error);
	});
}


/* ----- Mode selection ---------------------------------------------------- */


var current_mode = "active";


function activate_mode(name, on)
{
	if (on) {
		class_toggle(name, "active", true);
		class_toggle("menu-" + name, "active", true);
	} else {
		class_toggle(name, "active", false);
		class_toggle("menu-" + name, "active", false);
	}
}


function select_mode(name)
{
	activate_mode(current_mode, 0);
	current_mode = name;
	activate_mode(current_mode, 1);
}


/* ----- initialization ---------------------------------------------------- */


{
	for (let name of [ "active", "test" ]) {
		var td = document.getElementById("menu-" + name);

		td.addEventListener("click", function() { select_mode(name); });
		activate_mode(name, 0);
	}

	activate_mode("active", 1);

	poll_update();
}
