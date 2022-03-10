const baseHost = document.location.origin;

async function sleep(ms) {
	return new Promise(r => setTimeout(r, ms));
}

function debounce(func, timeout) {
	let handle;
	if (timeout) {
		return function (...args) {
			clearTimeout(handle);
			handle = setTimeout(() => func.apply(this, args), timeout);
		};
	}
	else {
		return function (...args) {
			window.cancelAnimationFrame(handle);
			handle = requestAnimationFrame(() => func.apply(this, args));
		}
	}
}

function colorForRSSI(value) {
	/**/ if (value > -65) return '#00A000';
	else if (value > -70) return '#76BA00';
	else if (value > -75) return '#889200';
	else if (value > -80) return '#AA8B00';
	else if (value > -85) return '#AA3C00';
	else if (value > -95) return '#D52800';
	else /*            */ return '#AA1111';
}

function colorForPing(value) {
	/**/ if (value < 50)   return '#00A000';
	else if (value < 100)  return '#76BA00';
	else if (value < 200)  return '#889200';
	else if (value < 333)  return '#AA8B00';
	else if (value < 500)  return '#AA3C00';
	else if (value < 1000) return '#D52800';
	else /*             */ return '#AA1111';
}

function handleFetchResult(promise, successMessage = 'Wysłano!', failureMessage = 'Wystąpił błąd podczas komunikacji z sterownikiem!') {
	return promise
		.then(response => {
			if (response && typeof response.status == 'number' && response.status >= 400) {
				throw new Error(`Serwer zwrócił błąd: HTTP ${response.status}`);
			}
			if (successMessage) {
				alert(successMessage);
			}
		})
		.catch(error => {
			if (failureMessage) {
				alert(`${failureMessage}\n\n${error.toString()}`);
			}
			console.error(error);
		})
	;
}





////////////////////////////////////////////////////////////////////////////////
// Colors

const colors = ['red', 'blue', 'green', 'white'];
const colorsInputs = Object.fromEntries(colors.map(c => [c, document.querySelector(`#colors .grid input[type=number][name=${c}]`)]))
const colorsSliders = Object.fromEntries(colors.map(c => [c, document.querySelector(`#colors .grid input[type=range][name=${c}]`)]));

// Bind sliders to raw values
for (const color of colors) {
	const slider = colorsInputs[color];
	const number = colorsSliders[color];
	slider.addEventListener('input', () => { number.value = slider.value; tryPreviewColors(); });
	number.addEventListener('input', () => { slider.value = number.value; tryPreviewColors(); });
}

// Colors previewing
const previewColorsCheckbox = document.querySelector('#colors input[name=preview]');
function isPreviewingColors() {
	return previewColorsCheckbox.checked;
}
async function setPreviewColors(checked) {
	previewColorsCheckbox.checked = checked;
	await fetch(`${baseHost}/config?forceColors=${checked}`);
}
previewColorsCheckbox.addEventListener('input', function () {
	fetch(`${baseHost}/config?forceColors=${this.checked}`);
	if (!this.checked) {
		clearTimeout(previewColorsOnChangeTimeout);
	}
})

let previewColorsOnChangeTimeout = 0;
async function tryPreviewColors() {
	const restartTimeout = () => {
		clearTimeout(previewColorsOnChangeTimeout);
		previewColorsOnChangeTimeout = setTimeout(() => {
			setPreviewColors(false);
			previewColorsOnChangeTimeout = 0;
		}, 3333);
	}
	if (isPreviewingColors()) {
		sendColorsPreview();
		if (previewColorsOnChangeTimeout) {
			restartTimeout();
		}
	}
	else {
		if (document.querySelector('#colors input[name=previewOnChange]').checked) {
			await setPreviewColors(true);
			sendColorsPreview();
			restartTimeout();
		}
	}
}

// Color preview pushing
let colorsPreviewPending;
function sendColorsPreview() {
	const querystring = Object.entries(colorsInputs).map(([c, e]) => `${c}=${e.value}`).join('&');
	if (!colorsPreviewPending) {
		const send = (querystring) => {
			if (!isPreviewingColors()) {
				colorsPreviewPending = undefined;
			}
			fetch(`${baseHost}/config?${querystring}`).finally(() => {
				if (colorsPreviewPending == querystring) {
					colorsPreviewPending = undefined;
				}
				else {
					send(colorsPreviewPending);
				}
			})
		}
		send(querystring);
	}
	colorsPreviewPending = querystring;
};

class ColorsCycleManager {
	static MAX_ENTRIES = 14;
	
	constructor(container) {
		this.container = container;
		this.tbody = this.container.querySelector('tbody');
		this.template = this.tbody.querySelector('template').content;
	}

	get count() {
		return this.tbody.childElementCount;
	}

	addRow(data) {
		if (ColorsCycleManager.MAX_ENTRIES <= this.count) {
			throw new Error(`Nie może być więcej niż ${ColorsCycleManager.MAX_ENTRIES} wpisów.`);
		}

		const fragment = this.template.cloneNode(true);
		const row = fragment.querySelector('tr');

		row.querySelector('input[type=time]').value = data.time;
		for (const color of colors) {
			row.querySelector(`input[name=${color}]`).value = data[color];
		}

		// TODO: buttons handling
		const that = this;
		row.querySelector('button[name=remove]').addEventListener('click', () => row.remove());
		row.querySelector('button[name=move-up]').addEventListener('click', () => {
			const previous = row.previousElementSibling;
			const parent = row.parentElement;
			if (previous) {
				parent.removeChild(row);
				parent.insertBefore(row, previous);
			}
		});
		row.querySelector('button[name=move-down]').addEventListener('click', () => {
			const next = row.nextElementSibling;
			const parent = row.parentElement;
			if (next) {
				parent.removeChild(row);
				if (next.nextElementSibling) {
					parent.insertBefore(row, next.nextElementSibling);
				}
				else {
					parent.appendChild(row);
				}
			}
		});
		row.querySelector('button[name=clone]').addEventListener('click', () => that.cloneRow(row));
		row.querySelector('button[name=preview]').addEventListener('click', () => {
			for (const color of colors) {
				colorsInputs[color].value = colorsSliders[color].value = row.querySelector(`input[name=${color}]`).value;
			}
			tryPreviewColors();
		});
		row.querySelector('button[name=set-from-current]').addEventListener('click', () => {
			for (const color of colors) {
				row.querySelector(`input[name=${color}]`).value = colorsInputs[color].value;
			}
		});

		this.tbody.appendChild(fragment);
		return row;
	}

	cloneRow(row) {
		const clone = this.addRow(this.getRowData(row));
		clone.remove();
		row.after(clone);
		return clone;
	}

	getRowData(row) {
		const data = {
			time: row.querySelector('input[type=time]').value,
		};
		for (const color of colors) {
			data[color] = row.querySelector(`input[name=${color}]`).value;
		}
		return data;
	}

	getCurrentData() {
		const data = {
			time: new Date().toTimeString().substring(0, 5),
		}
		for (const [color, input] of Object.entries(colorsInputs)) {
			data[color] = parseInt(input.value);
		}
		return data;
	}

	async upload() {
		if (this.tbody.childElementCount < 1) {
			throw new Error('Musi być co najmniej jeden wpis!');
		}
		const body = [...this.tbody.children]
			.map(row => this.getRowData(row))
			.map(data => `#${data.red},${data.green},${data.blue},${data.white}@${data.time}`)
		;
		return fetch(`${baseHost}/setColorsCycle`, {
			method: 'POST',
			body: body,
		});
	}

	async download() {
		return fetch(`${baseHost}/getColorsCycle`)
			.then(response => response.json())
			.then(points => {
				for (const row of [...this.tbody.children]) {
					row.remove();
				}
				for (const data of points) {
					this.addRow(data);
				}
			})
		;
	}

	async reset() {
		return fetch(`${baseHost}/resetColorsCycle`).then(() => this.download());
	}
}

const colorsCycleManager = new ColorsCycleManager(document.querySelector('#colors .colors-cycle'));

document.querySelector('#colors button[name=add-from-current]').addEventListener('click', () => {
	colorsCycleManager.addRow(colorsCycleManager.getCurrentData());
});
document.querySelector('#colors button[name=upload]').addEventListener('click', () => {
	handleFetchResult(
		colorsCycleManager.upload(), 
		`Wysłano!\n\nWysłano ustawienia cyklu oświetlenia, będą one aktywne aż do resetu sterownika. Zapisz do EEPROM, jeśli chcesz zapisać zmiany między uruchomieniami.`,
	);
});
document.querySelector('#colors button[name=download]').addEventListener('click', () => {
	if (!window.confirm("Jesteś pewien, że chcesz pobrać ustawienia? Nadpisze to nie zapisane zmiany.")) {
		return;
	}
	handleFetchResult(
		colorsCycleManager.download(),
		`Pobrano!`,
	);
});
document.querySelector('#colors button[name=reset]').addEventListener('click', () => {
	if (!window.confirm("Jesteś pewien, że chcesz zresetować ustawienia dobowego cyklu oświetlania? Usunie to wszystkie obecnie ustawione punkty.")) {
		return;
	}
	handleFetchResult(
		colorsCycleManager.reset(),
		`Zresetowane!`,
	);
});





////////////////////////////////////////////////////////////////////////////////
// Status

// Status updating
(async () => {
	let lastPing = 0;
	async function updateStatus() {
		const pingOutput = document.querySelector('output[name=ping]');
		const rssiOutput = document.querySelector('output[name=rssi]');
		const start = new Date().getTime();
		let end;
		await fetch(`${baseHost}/status`)
			.then(response => {
				end = new Date().getTime();
				return response.json();
			})
			.then(state => {
				for (const key in state) {
					const value = state[key];
					const previewColors = document.querySelector('#colors input[name=preview]').checked;
					switch (key) {
						case 'timestamp': {
							const b = value.split(/\D/);
							const d = new Date(b[0], b[1]-1, b[2], b[3], b[4], b[5]);
							document.querySelector('output[name=datetime]').innerText = d.toLocaleString();
							break;
						}
						case 'waterTemperature':
						case 'airTemperature':
							document.querySelector(`output[name=${key}]`).innerText = value + '°C';
							break;
						case 'phLevel':
							document.querySelector('output[name=phLevel]').innerText = value;
							break;
						case 'rssi': {
							rssiOutput.classList.remove('error');
							rssiOutput.style.color = colorForRSSI(value);
							rssiOutput.innerText = value + 'dB';
							break;
						}
						case 'red':
						case 'green':
						case 'blue':
						case 'white':
							if (!isPreviewingColors()) {
								colorsInputs[key].value = colorsSliders[key].value = value;
							}
							break;
						case 'isHeating':
							document.querySelector(`output[name=heating]`).value = value ? '🔥↗' : '❄↘';
							break;
						case 'isRefilling':
							document.querySelector(`output[name=waterLevel]`).value = value ? '💧💦↗' : 'OK';
							break;
						case 'isRefillTankLow':
							document.querySelector('output[name=refillTankLevel]').value = value ? 'Niski' : 'OK';
							break;
					}
				}

				lastPing = end - start;
				pingOutput.classList.remove('error');
				pingOutput.style.color = colorForPing(lastPing);
				pingOutput.innerText = `${lastPing}ms`;
			})
			.catch(error => {
				pingOutput.classList.add('error');
				pingOutput.style.color = '';
				pingOutput.innerText = `Brak połączenia!`;
				rssiOutput.classList.add('error');
				rssiOutput.style.color = '';
				rssiOutput.innerText = `?`;
				throw error;
			})
		;
	}
	while (true) {
		try {
			await updateStatus();
			await sleep(1000 - lastPing);
		}
		catch (error) {
			console.error(error);
			await sleep(3333);
		}
	}
})();





////////////////////////////////////////////////////////////////////////////////
// Settings

const byValueSettings = [
	'heatingMinTemperature', 
	'heatingMaxTemperature', 
	'circulatorActiveTime', 
	'circulatorPauseTime',
	'cloudLoggingInterval',
];
const byValueSettingsInputs = Object.fromEntries(byValueSettings.map(name => [name, document.querySelector(`input[name=${name}]`)]));

function saveSettings() {
	const querystring = Object.entries(byValueSettingsInputs).map(([name, element]) => {
		switch (element.type) {
			case 'checkbox': {
				return `${name}=${element.checked}`;
			}
			case 'time': {
				const h = parseInt(element.value);
				const m = parseInt(element.value.substring(3));
				const s = parseInt(element.value.substring(6)) || 0;
				return `${name}=${((h * 60) + m) * 60 + s}`;
			}
			default: {
				return `${name}=${element.value}`;
			}
		}
	}).join('&');
	return fetch(`${baseHost}/config?${querystring}`);
};

// Save settings on change, debounced 
const debouncedSaveSettings = debounce(saveSettings, 1000);
for (const input of Object.values(byValueSettingsInputs)) {
	input.addEventListener('change', debouncedSaveSettings);
}
byValueSettingsInputs['cloudLoggingInterval'].addEventListener('change', function () {
	restartChartsUpdate(parseInt(this.value));
})

document.querySelector('button[name=save-eeprom]').addEventListener('click', () => {
	const promise = Promise.all([
		colorsCycleManager.upload(),
		saveSettings(),
	])
		.then(() => fetch(`${baseHost}/saveEEPROM`))
		.then(defaultSendRequestResponseHandler)
		.catch(defaultSendRequestErrorHandler)
	;
	handleFetchResult(promise);
});

document.querySelector('button[name=synchronize-time]').addEventListener('click', () => {
	const date = new Date();
	const y = date.getFullYear();
	const M = date.getMonth() + 1;
	const d = date.getDate();
	const H = date.getHours();
	const m = date.getMinutes();
	const s = date.getSeconds();
	const string = `${y}-${M>9?M:'0'+M}-${d>9?d:'0'+d}T${H>9?H:'0'+H}:${m>9?m:'0'+m}:${s>9?s:'0'+s}`;
	handleFetchResult(
		fetch(`${baseHost}/config?timestamp=${string}`)
	)
});

// Load settings
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
					byValueSettingsInputs[key].value = parseFloat(value).toFixed(2);
					break;
				case 'circulatorActiveTime':
				case 'circulatorPauseTime':
				case 'cloudLoggingInterval':
					const seconds = parseInt(value);
					byValueSettingsInputs[key].value = (
						`${Math.floor(seconds / 3600)}`.padStart(2, '0') + ':' +
						`${Math.floor(seconds % 3600 / 60)}`.padStart(2, '0') + ':' +
						`${seconds % 60}`.padStart(2, '0')
					);
					if (key == 'cloudLoggingInterval') {
						restartChartsUpdate(seconds);
					}
					break;
			}
		}
	})
	.catch((error) => {
		alert(`Błąd połączenia! Nie można załadować ustawień. Odśwież stronę...`);
		console.error(error);
	})
;
colorsCycleManager.download();





////////////////////////////////////////////////////////////////////////////////
// Other

// Update chart periodically
let chartsUpdateInterval = 0;
function restartChartsUpdate(seconds = 10) {
	clearTimeout(chartsUpdateInterval);
	chartsUpdateInterval = setInterval(() => {
		updateChart();
	}, seconds * 1000);
}




