const $  = (s,o) => (o || document).querySelector(s);
const $$ = (s,o) => (o || document).querySelectorAll(s);

const baseHost = document.location.origin;
var lastError;

const defaultSendRequestResponseHandler = (response) => {
	if (response.status >= 400) {
		throw `Serwer zwrócił błąd ${response.status}`;
	}
	alert('Wysłano!');
};
const defaultSendRequestErrorHandler = (error) => {
	alert('Wystąpił błąd podczas wysyłania!');
	console.error(error);
};





/* COLORS */
const colorIDs = ['red', 'blue', 'green', 'white'];

// Sliders settings push
let slidersSettingPushTimeout = 0;
const tryPushSlidersSettings = () => {
	clearTimeout(slidersSettingPushTimeout);
	slidersSettingPushTimeout = setTimeout(() => {
		const querystring = colorIDs.map((name) => `${name}=${$(`#${name}`).value}`).join('&');
		fetch(`${baseHost}/config?${querystring}`).then(() => {
			clearTimeout(slidersSettingPushTimeout); 
			// It mean there is no pushing in progress (and clear just to be sure)
			slidersSettingPushTimeout = 0;
		})
	}, 333);
};
// Bind sliders to raw values
colorIDs.forEach((name) => {
	let slider = $(`#${name}-slider`);
	let number = $(`#${name}`);
	
	slider.oninput = () => {number.value = slider.value;tryPushSlidersSettings();};
	number.oninput = () => {slider.value = number.value;tryPushSlidersSettings();};
});

const setForceColors = (checked) => fetch(`${baseHost}/config?forceColors=${checked}`);

const rowRemove = (row) => {
	row.remove();
};
const rowUp = (row) => {
	let previous = row.previousElementSibling;
	let parent = row.parentElement;
	if (previous) {
		parent.removeChild(row);
		parent.insertBefore(row, previous);
	}
};
const rowDown = (row) => {
	let next = row.nextElementSibling;
	let parent = row.parentElement;
	if (next) {
		parent.removeChild(row);
		if (next.nextElementSibling) {
			parent.insertBefore(row, next.nextElementSibling);
		}
		else {
			parent.appendChild(row);
			console.log(row);
		}
	}
};
const rowClone = (row) => {
	colorsCycle.cloneRow(row);
};
const rowShowOnSliders = (row) => {
	colorIDs.forEach((name) => {
		$(`#${name}`).value = $(`#${name}-slider`).value = $(`input[name=${name}]`, row).value;
	});
	tryPushSlidersSettings();
};
const setFromCurrent = (row) => {
	$('input[type=time]',  row).value = new Date().toTimeString().substring(0, 5);
	colorIDs.forEach((name) => {
		$(`input[name=${name}]`, row).value = $(`#${name}`).value;
	});
};

var colorsCycle = {
	tableBody: $('.colors-cycle > table > tbody'),
	rowTemplate: undefined,

	MaxEntries: 14,

	addRow: function (data) {
		if (this.tableBody.childElementCount >= this.MaxEntries) {
			alert(`Nie może być więcej niż ${this.MaxEntries} wpisów.`);
			return null;
		}
		let row = this.rowTemplate.cloneNode(true);
		row.classList.remove('template');
		$('input[type=time]',  row).value = data.time;
		colorIDs.forEach((name) => {
			$(`input[name=${name}]`, row).value = data[name];
		});
		this.tableBody.appendChild(row);
		return row;
	},
	cloneRow: function (row) {
		if (this.tableBody.childElementCount >= this.MaxEntries) {
			alert(`Nie może być więcej niż ${this.MaxEntries} wpisów.`);
			return;
		}
		row.parentElement.insertBefore(row.cloneNode(true), row);
	},
	addFromCurrent: function () {
		this.addRow(
			Object.fromEntries([
				['time', new Date().toTimeString().substring(0, 5)],
				...(colorIDs.map((name) => [name, $(`#${name}`).value]))
			])
		);
	},
	removeAllRows: function () {
		let newBody = this.tableBody.cloneNode(false);
		this.tableBody.parentElement.replaceChild(newBody, this.tableBody);
		this.tableBody = newBody;
	},

	/// Downloads and renders current colors cycle setting
	download: function (silent) {
		if (!silent && !window.confirm("Jesteś pewien, że chcesz pobrać ustawienia? Nadpisze to wysłane zmiany.")) {
			return;
		}
		fetch(`${baseHost}/getColorsCycle`)
			.then((response) => {
				return response.json();
			})
			.then((points) => {
				this.removeAllRows();
				points.forEach(this.addRow.bind(this));
			})
			.catch((error) => {
				if (!silent) {
					alert('Wystąpił błąd podczas pobierania!');
				}
				console.error(error);
				lastError = error;
			});
	},
	upload: function ({silent} = {}) {
		if (colorsCycle.tableBody.childElementCount < 1) {
			if (!silent) {
				alert('Musi być co najmniej jeden wpis!');
			}
			return Promise.resolve(false);
		}
		let body = '';
		for (let row of this.tableBody.children) {
			if (row.classList.contains('template')) {
				continue;
			}
			// Format: "#R,G,B@HH:mm"
			let t = $('input[type=time]', row).value;
			let r = $('input[name=red]', row).value;
			let g = $('input[name=green]', row).value;
			let b = $('input[name=blue]', row).value;
			let w = $('input[name=white]', row).value;
			body += `#${r},${g},${b},${w}@${t}`;
		}
		let promise = fetch(`${baseHost}/pushColorsCycle`, {
			method: 'POST',
			body: body,
		});
		if (!silent) {
			promise = promise
				.then(defaultSendRequestResponseHandler)
				.catch(defaultSendRequestResponseHandler)
			;
		}
		return promise;
	},
	reset: function () {
		if (!window.confirm("Jesteś pewien, że chcesz zresetować ustawienia dobowego cyklu oświetlania? Usunie to wszystkie obecnie ustawione punkty.")) {
			return;
		}
		fetch(`${baseHost}/resetColorsCycle`)
			.then(() => {
				this.download();
			})
		;
	},
};
colorsCycle.rowTemplate = colorsCycle.tableBody.querySelector('.template');
colorsCycle.tableBody.removeChild(colorsCycle.rowTemplate);





/* STATUS */
const synchronizeTime = () => {
	const date = new Date();
	const y = date.getFullYear();
	const M = date.getMonth() + 1;
	const d = date.getDate();
	const H = date.getHours();
	const m = date.getMinutes();
	const s = date.getSeconds();
	const string = `${y}-${M>9?M:'0'+M}-${d>9?d:'0'+d}T${H>9?H:'0'+H}:${m>9?m:'0'+m}:${s>9?s:'0'+s}`;
	fetch(`${baseHost}/config?timestamp=${string}`);
};

const updateStatus = () => {
	const start = new Date().getTime();
	let end;
	fetch(`${baseHost}/status`)
		.then((response) => {
			end = new Date().getTime();
			return response.json();
		})
		.then((state) => {
			for (let key in state) {
				const value = state[key];
				switch (key) {
					case 'timestamp': {
						const b = value.split(/\D/);
						const d = new Date(b[0], b[1]-1, b[2], b[3], b[4], b[5]);
						$('#datetime').innerText = d.toLocaleString();
						break;
					}
					case 'waterTemperature':
					case 'airTemperature':
						$('#' + key).innerText = value + '°C';
						break;
					case 'phLevel':
						$('#phLevel').innerText = value;
						break;
					case 'rssi': {
						let o = $('#rssi output');
						o.innerText = value + 'dB';
						/**/ if (value > -65) o.style.color = '#00A000';
						else if (value > -70) o.style.color = '#76BA00';
						else if (value > -75) o.style.color = '#889200';
						else if (value > -80) o.style.color = '#AA8B00';
						else if (value > -85) o.style.color = '#AA3C00';
						else if (value > -95) o.style.color = '#D52800';
						else /*            */ o.style.color = '#AA1111';
						break;
					}
					case 'red':
					case 'green':
					case 'blue':
					case 'white':
						if (slidersSettingPushTimeout == 0) {
							$(`#${key}`).value = $(`#${key}-slider`).value = value;
						}
						break;
					case 'isHeating':
						$(`#${key}`).value = value ? '🔥↗' : '❄↘';
						break;
					case 'isRefilling':
						$(`#waterLevel`).value = value ? '💧💦↗' : 'OK';
						break;
					case 'isRefillTankLow':
						$('#refillTankLevel').value = value ? 'Niski' : 'OK';
						break;
				}
			}

			$('#ping').innerText = `Ping: ${end - start}ms`;
			$('#ping').classList.remove('error');
		})
		.catch((error) => {
			$('#ping').innerText = 'Brak połączenia!';
			$('#ping').classList.add('error');
			$('#rssi output').innerText = '';
			console.error(error);
		});
	;
};





/* BY-VALUE SETTINGS */
byValueSettings = [
	'heatingMinTemperature', 
	'heatingMaxTemperature', 
	'circulatorActiveTime', 
	'circulatorPauseTime',
	'cloudLoggingInterval',
];
const pushByValueSettings = ({silent} = {}) => {
	let querystring = byValueSettings.map((name) => {
		let element = $(`#${name}`);
		if (element.type == 'checkbox') {
			return `${name}=${element.checked}`;
		}
		if (element.type == 'time') {
			let h = parseInt(element.value);
			let m = parseInt(element.value.substring(3));
			let s = parseInt(element.value.substring(6)) || 0;
			return `${name}=${((h * 60) + m) * 60 + s}`;
		}
		return `${name}=${element.value}`;
	}).join('&');
	let promise = fetch(`${baseHost}/config?${querystring}`).then(() => {
		clearTimeout(byValueSettingsPushTimeout); 
		// It mean there is no pushing in progress (and clear just to be sure)
		byValueSettingsPushTimeout = 0;
	});
	if (!silent) {
		promise = promise
			.then(defaultSendRequestResponseHandler)
			.catch(defaultSendRequestResponseHandler)
		;
	}
	return promise;
};
let byValueSettingsPushTimeout = 0;
const tryPushByValueSettings = () => {
	clearTimeout(byValueSettingsPushTimeout);
	byValueSettingsPushTimeout = setTimeout(() => {
		pushByValueSettings({silent: true});
	}, 1000);
};
byValueSettings.forEach((name) => {
	let element = $(`#${name}`);
	element.onchange = tryPushByValueSettings;
});





/* SAVE TO EEPROM */
const saveSettingsToEEPROM = () => {
	return Promise.all([
		colorsCycle.upload({silent: true}),
		pushByValueSettings({silent: true}),
	])
		.then(() => fetch(`${baseHost}/saveEEPROM`))
		.then(defaultSendRequestResponseHandler)
		.catch(defaultSendRequestResponseHandler)
	;
};





/* INITIAL DATA FETCH */
updateStatus();
fetch(`${baseHost}/config`)
	.then((response) => {
		return response.json();
	})
	.then((state) => {
		for (let key in state) {
			const value = state[key];
			switch (key) {
				case 'heatingMinTemperature':
				case 'heatingMaxTemperature':
					$(`#${key}`).value = parseFloat(value).toFixed(2);
					break;
				case 'circulatorActiveTime':
				case 'circulatorPauseTime':
				case 'cloudLoggingInterval':
					const seconds = parseInt(value);
					$(`#${key}`).value = (
						`${Math.floor(seconds / 3600)}`.padStart(2, '0') + ':' +
						`${Math.floor(seconds % 3600 / 60)}`.padStart(2, '0') + ':' +
						`${seconds % 60}`.padStart(2, '0')
					);
					break;
			}
		}
	})
	.catch((error) => {
		alert('Błąd połączenia! Odśwież stronę...');
		console.error(error);
	})
;
colorsCycle.download(true);

let statusUpdateInterval = setInterval(updateStatus, 1000);
let chartsUpdateInterval = setInterval(() => {
	updateChart();
}, 10000);
