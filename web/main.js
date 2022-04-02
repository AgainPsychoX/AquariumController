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
			return response;
		})
		.catch(error => {
			if (failureMessage) {
				alert(`${failureMessage}\n\n${error.toString()}`);
			}
			console.error(error);
		})
	;
}

document
	.querySelectorAll('dialog')
	.forEach(dialog => {
		new MutationObserver((list, observer) => {
			for (const mutation of list) {
				if (typeof(mutation.oldValue) == 'object') {
					dialog.classList.add('open');
				}
			}
		}).observe(dialog, { attributeFilter: ['open'], attributeOldValue: true })
		dialog.addEventListener('cancel', e => {
			if (dialog.classList.contains('closing')) return;
			e.preventDefault();
			dialog.classList.add('closing');
		});
		dialog.addEventListener('transitionend', e => {
			if (dialog.classList.contains('closing')) {
				dialog.close();
				dialog.classList.remove('open');
				dialog.classList.remove('closing');
			}
		});
	})
;





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
const previewColorsOnChange = {
	duration: Infinity,
	checkbox: false,
	timeout: 0,
}

const previewOnChangeCheckbox = document.querySelector('#colors input[name=previewOnChange]');
if (!previewColorsOnChange.checkbox) {
	previewOnChangeCheckbox.style.display = 'none';
}

const manualColorsCheckbox = document.querySelector('#colors input[name=manual]');
function isManualColors() {
	return manualColorsCheckbox.checked;
}
async function setManualColors(checked) {
	manualColorsCheckbox.checked = checked;
	await fetch(`${baseHost}/config?forceColors=${checked}`);
}
manualColorsCheckbox.addEventListener('input', function () {
	fetch(`${baseHost}/config?forceColors=${this.checked}`);
	if (!this.checked) {
		clearTimeout(previewColorsOnChange.timeout);
	}
})

async function tryPreviewColors() {
	const restartTimeout = () => {
		if (!isFinite(previewColorsOnChange.duration)) return;
		clearTimeout(previewColorsOnChange.timeout);
		previewColorsOnChange.timeout = setTimeout(() => {
			setManualColors(false);
			previewColorsOnChange.timeout = 0;
		}, previewColorsOnChange.duration);
	}
	if (isManualColors()) {
		sendColorsPreview();
		if (previewColorsOnChange.timeout) {
			restartTimeout();
		}
	}
	else {
		if (previewOnChangeCheckbox.checked) {
			await setManualColors(true);
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
			if (!isManualColors()) {
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
			time: row.querySelector('input[type=time]').value.substring(0, 5),
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
			.join('')
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

const booleanStatusBits = [
	{key: 'isHeating',       output: 'heating',         true: '🔥↗', false: '❄↘'},
	{key: 'isRefilling',     output: 'waterLevel',      true: '💧💦↗', false: 'OK'},
	{key: 'isRefillTankLow', output: 'refillTankLevel', true: 'Niski', false: 'OK'},
];

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
					switch (key) {
						case 'timestamp': {
							const b = value.split(/\D/);
							const d = new Date(b[0], b[1]-1, b[2], b[3], b[4], b[5]);
							document.querySelector('output[name=datetime]').innerText = d.toLocaleString();
							break;
						}
						case 'waterTemperature':
						case 'rtcTemperature':
							document.querySelector(`output[name=${key}]`).innerText = value + '°C';
							break;
						case 'phLevel': {
							const adc = state['phRaw'];
							const output = document.querySelector('output[name=phLevel]');
							output.innerText = value.toFixed(2); 
							output.title = `pH: ${value} adc: ${adc} (avg)`;
							if (phCalibrationDialog.open) {
								phCalibrationDialog.update({ adc, pH: value })
							}
							break;
						}
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
							if (!isManualColors()) {
								colorsInputs[key].value = colorsSliders[key].value = value;
							}
							break;
					}
				}

				for (const entry of booleanStatusBits) {
					if (typeof state[entry.key] != undefined) {
						const value = !!state[entry.key];
						const output = document.querySelector(`output[name=${entry.output}]`);
						output.value = entry[value];
						output.classList.toggle('active', value);
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

function parseTimeInput(input) {
	const h = parseInt(input.value);
	const m = parseInt(input.value.substring(3));
	const s = parseInt(input.value.substring(6)) || 0;
	return ((h * 60) + m) * 60 + s;
}

function saveByValueSettings() {
	const querystring = Object.entries(byValueSettingsInputs).map(([name, element]) => {
		switch (element.type) {
			case 'checkbox': {
				return `${name}=${element.checked}`;
			}
			case 'time': {
				return `${name}=${parseTimeInput(element)}`;
			}
			default: {
				return `${name}=${element.value}`;
			}
		}
	}).join('&');
	return fetch(`${baseHost}/config?${querystring}`);
};

// Save by-value settings on change, debounced 
const debouncedSaveByValueSettings = debounce(saveByValueSettings, 1000);
for (const input of Object.values(byValueSettingsInputs)) {
	input.addEventListener('change', debouncedSaveByValueSettings);
}
byValueSettingsInputs['cloudLoggingInterval'].addEventListener('change', function () {
	restartChartsUpdate(parseTimeInput(this));
})

const minerals = [
	{key: 'Ca', name: 'Chlorek wapnia CaCl₂'},
	{key: 'Mg', name: 'Chlorek magnezu MgCl₂'},
	{key: 'KH', name: 'Wodorowęglan sodu NaHCO₃'},
];
function saveMineralsPumpsSettings() {
	const querystring = minerals.map(mineral => {
		const key = mineral.key.toLowerCase();
		const div = document.querySelector(`#minerals .grid div[name=${key}]`)
		return (
			`mineralsPumps.${key}.time=${parseTimeInput(div.querySelector('input[type=time]')) / 60}&` +
			`mineralsPumps.${key}.mL=${div.querySelector('input[type=number]').value}`
		);
	}).join('&');
	return fetch(`${baseHost}/config?${querystring}`);
}
const debouncedSaveMineralsPumpsSettings = debounce(saveMineralsPumpsSettings, 1000);

document.querySelector('button[name=save-eeprom]').addEventListener('click', async () => {
	const promise = Promise.resolve()
		.then(() => colorsCycleManager.upload())
		.then(saveByValueSettings)
		.then(saveMineralsPumpsSettings)
		.then(() => fetch(`${baseHost}/saveEEPROM`))
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
	handleFetchResult(fetch(`${baseHost}/config?timestamp=${string}`));
});

const phCalibrationDialog = document.querySelector('dialog#ph-calibration');
let phMeter;
phCalibrationDialog.getPoints = () => {
	const points = [];
	for (const row of phCalibrationDialog.querySelectorAll('.points tbody tr').values()) {
		points.push({
			adc: parseInt(row.querySelector('input[name=adc]').value),
			pH: parseFloat(row.querySelector('input[name=pH]').value),
		});
	}
	return points.sort((a, b) => a.adc - b.adc);
};
phCalibrationDialog.update = (status) => {
	const toVoltage = adc => adc / phMeter.adcMax * phMeter.adcVoltage;
	const calculate = (adc, points) => {
		points ||= phMeter.points;
		const i = adc <= points[1].adc ? 0 : 1;
		const p0 = points[i + 0];
		const p1 = points[i + 1];
		const {y0, x0} = { y0: p0.pH, x0: p0.adc };
		const {y1, x1} = { y1: p1.pH, x1: p1.adc };
		const a = (y0 - y1) / (x0 - x1);
		const b = y0 - a * x0;
		return a * adc + b;
	};

	const newPoints = phCalibrationDialog.getPoints();
	
	phCalibrationDialog.readings.normal = {
		adc: status.adc,
		voltage: toVoltage(status.adc).toFixed(2) + 'V',
		pH: calculate(status.adc).toFixed(2),
		pH_new: calculate(status.adc, newPoints).toFixed(2),
	};

	const samples = phCalibrationDialog.samples;
	samples.push(status.adc);
	const average = Math.round(samples.reduce((a, b) => a + b) / samples.length);
	phCalibrationDialog.readings.average = {
		adc: average,
		voltage: toVoltage(average).toFixed(2) + 'V',
		pH: calculate(average).toFixed(2),
		pH_new: calculate(average, newPoints).toFixed(2),
	};

	for (const row of phCalibrationDialog.querySelectorAll('.readings tbody tr').values()) {
		const source = row.querySelector('input[name=source]').value;
		for (const name of ['adc', 'voltage', 'pH', 'pH_new']) {
			row.querySelector(`output[name=${name}]`).innerText = phCalibrationDialog.readings[source][name];
		}
	}
};
{
	const openButton = document.querySelector('button[name=ph-calibrate]');
	openButton.addEventListener('click', () => {
		const promise = fetch(`${baseHost}/config`)
			.then((response) => response.json())
			.then((state) => {
				const template = phCalibrationDialog.querySelector('template');
				const tbody = template.parentElement;
				tbody.replaceChildren(template);
				for (let i = 0; i < 3; i++) {
					const data = state.phMeter.points[i];
					const fragment = template.content.cloneNode(true);
					fragment.querySelector('td').innerText = `${i + 1}.`;
					const adcInput = fragment.querySelector('input[name=adc]');
					const pHInput =  fragment.querySelector('input[name=pH]');
					adcInput.value = data.adc;
					pHInput.value = data.pH;
					fragment.querySelector('button').addEventListener('click', e => {
						e.preventDefault();
						const source = phCalibrationDialog.querySelector('input[name=source]:checked').value;
						adcInput.value = phCalibrationDialog.readings[source].adc;
					})
					tbody.append(fragment);
				}
				phMeter = state.phMeter;

				phCalibrationDialog.showModal();
			})
		;
		handleFetchResult(promise, '');
	});
	
	phCalibrationDialog.samples = [];
	phCalibrationDialog.readings = {};
	
	phCalibrationDialog.querySelector('button[name=save]').addEventListener('click', async (e) => {
		phCalibrationDialog.classList.add('closing');

		const querystring = phCalibrationDialog.getPoints()
			.map((p, i) => `phMeter.points.${i}.adc=${p.adc}&phMeter.points.${i}.pH=${p.pH}`)
			.join('&')
		;
		await handleFetchResult(fetch(`${baseHost}/config?${querystring}`));
	});
	phCalibrationDialog.querySelector('button[name=cancel]').addEventListener('click', async (e) => {
		phCalibrationDialog.classList.add('closing');
	});
}

// Load settings
fetch(`${baseHost}/config`)
	.then((response) => response.json())
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

		// Mineral dosing fields
		const mineralsGrid = document.querySelector('#minerals .grid');
		const mineralsTemplate = document.querySelector('#minerals .grid template').content;
		for (const mineral of minerals) {
			const fragment = mineralsTemplate.cloneNode(true);
			const key = mineral.key.toLowerCase();
			fragment.querySelector('div.item').setAttribute('name', key);
			fragment.querySelector('h3 abbr').innerText = mineral.key;
			fragment.querySelector('h3 small').innerText = `(${mineral.name})`;
			const minutes = state['mineralsPumps'][key]['time'];
			const timeInput   = fragment.querySelector('input[type=time]');
			const numberInput = fragment.querySelector('input[type=number]');
			timeInput.value = [`${Math.floor(minutes / 60)}`, `${minutes % 60}:00`].map(p => p.padStart(2, '0')).join(':');
			numberInput.value = state['mineralsPumps'][key]['mL'];
			[timeInput, numberInput].forEach(input => input.addEventListener('change', () => {
				debouncedSaveMineralsPumpsSettings();
			}));

			const manualDoseButton = fragment.querySelector('button[name=manual-dose]');
			manualDoseButton.addEventListener('click', async () => {
				manualDoseButton.disabled = true;
				await saveMineralsPumpsSettings();
				await handleFetchResult(fetch(`${baseHost}/mineralsPumps?key=${key}&action=dose`));
				manualDoseButton.disabled = false;
			});

			const calibrateButton = fragment.querySelector('button[name=calibrate]');
			calibrateButton.addEventListener('click', async () => {
				calibrateButton.disabled = true;
				const mL = parseInt(prompt("Podaj ilość mililitrów. Po zatwierdzeniu rozpocznie się pompowanie.", 1000));
				if (isNaN(mL)) {
					calibrateButton.disabled = false;
					return;
				}

				await handleFetchResult(fetch(`${baseHost}/mineralsPumps?key=${key}&action=on`), false);

				alert("Pompowanie trwa. Kliknij OK po przepompowaniu wyznaczonej ilości.");

				const stats = await handleFetchResult(fetch(`${baseHost}/mineralsPumps?key=${key}&action=off`), false).then(r => r.json());
				const pump = stats.mineralsPumps[key];
				const duration = stats.currentTime - pump.lastStartTime;
				const c = duration / mL;
				
				if (confirm(`Pompa wyłączona.\nCzas: ${duration}\nmL: ${mL}\nStała kalibracji: ${c} (czas w ms dla 1 mL)\n\nZapisać?`)) {
					await handleFetchResult(fetch(`${baseHost}/config?mineralsPumps.${key}.c=${c}`));
				}
				
				calibrateButton.disabled = false;
			});

			mineralsGrid.appendChild(fragment);
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




