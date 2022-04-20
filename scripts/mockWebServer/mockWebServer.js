const express = require('express');
const bodyParser = require('body-parser');
const path = require('path');

const parseBoolean = (value, defaultValue) => {
	if (typeof value === 'undefined') {
		return defaultValue;
	}
	if (typeof value === 'boolean') {
		return value;
	}
	switch (value.toLowerCase().trim()) {
		case "true": case "yes": case "1": return true;
		case "false": case "no": case "0": return false;
		default: return defaultValue;
	}
}

const app = express();
app.use(express.json());
app.use(bodyParser.text());
app.use(express.static(path.join(__dirname, '../../web')));

const defaultColorCycle = [
	{red: 0, green: 0, blue: 0, white: 0, time: "00:00"},
	{red: 255, green: 255, blue: 255, white: 255, time: "12:00"},
];

const noise = (x, r = 0.3, e = 0.4, pi = 0.5) => Math.sin(x * r) + Math.sin(x * e * Math.E) + Math.sin(x * pi * Math.PI);
const speed = 1 / 1;
const config = {
	heatingMinTemperature: 21.75,
	heatingMaxTemperature: 24.25,
	circulatorActiveTime: 5,
	circulatorPauseTime: 7,
	cloudLoggingInterval: 1 * 60,
	forceColors: false,
	mineralsPumps: {
		ca: {time: 9 * 60 + 20, mL: 20},
		mg: {time: 9 * 60 + 25, mL: 25},
		kh: {time: 9 * 60 + 30, mL: 30},
	},
	phMeter: {
		points: [
			{ adc: 712, pH: 9.18 },
			{ adc: 851, pH: 6.86 },
			{ adc: 1008, pH: 4.01 },
		],
		adcMax: 1024,
		adcVoltage: 3.2,
	},
	network: {
		mode: 2,
		ssid: 'Hello world!',
		psk: '',
		static: 1,
		ip: '192.168.55.123',
		mask: 24,
		gateway: '192.168.55.1',
		dns1: '1.1.1.1',
		dns2: '1.0.0.1',
	}
};
let red = 127;
let green = 127;
let blue = 127;
let white = 127;
let colorsCycle = defaultColorCycle.map(e => Object.assign({}, e));
let airTemperature;
let waterTemperature = 22.0;
let phReal;
let phRaw;
let phLevel;
let isHeating = false;

const airToWaterTemperatureConversionRatio = 0.0001;
setInterval(() => {
	const x = Date.now() * speed;
	airTemperature = 22 + noise(x * 0.001) * 2;
	waterTemperature = (airTemperature * airToWaterTemperatureConversionRatio + waterTemperature) / (airToWaterTemperatureConversionRatio + 1),

	phReal = 7.6 + noise(x * 0.0001) / 3 / 2;
	phRaw = Math.round((phReal - 7) / 7 * 50 + 800);
	{
		const i = phRaw <= config.phMeter.points[1].adc ? 0 : 1;
		const p0 = config.phMeter.points[i + 0];
		const p1 = config.phMeter.points[i + 1];
		const {y0, x0} = { y0: p0.pH, x0: p0.adc };
		const {y1, x1} = { y1: p1.pH, x1: p1.adc };
		const a = (y0 - y1) / (x0 - x1);
		const b = y0 - a * x0;
		phLevel = a * phRaw + b;
	}

	if (config.heatingMaxTemperature < waterTemperature) {
		isHeating = false;
	}
	else if (waterTemperature < config.heatingMinTemperature) {
		isHeating = true;
	}
	if (isHeating) {
		waterTemperature += 0.01;
	}

	if (!config.forceColors) {
		// TODO: colors cycle
		red =   (7 * red + 0)     / 8;
		green = (7 * green + 64)  / 8;
		blue =  (7 * blue + 128)  / 8;
		white = (7 * white + 196) / 8;
	}
}, 500);

app.get('/status', (req, res) => {
	setTimeout(() => {
		res.status(200);
		res.json({
			waterTemperature: Math.round(waterTemperature * 100) / 100, // cut off .##, step = 0.01
			rtcTemperature: Math.round(airTemperature * 4) / 4, // step = 0.25
			phLevel: phLevel,
			phRaw: phRaw,
			red: Math.round(red),
			green: Math.round(green),
			blue: Math.round(blue),
			white: Math.round(white),
			isHeating,
			isRefilling: false,
			isRefillTankLow: false,
			timestamp: new Date().toJSON(),
			rssi: Math.round(-80 + noise(Date.now() * 0.02 * speed) / 3 * 30),
		});
	}, Math.max(0, Math.round(70 + noise(Date.now() * 0.02 * speed) / 3 * 50)))
});
app.get('/config', (req, res) => {
	if (req.query.timestamp) {
		// TODO: ...
	}
	if (req.query.red) {
		red = parseInt(req.query.red);
	}
	if (req.query.green) {
		green = parseInt(req.query.green);
	}
	if (req.query.blue) {
		blue = parseInt(req.query.blue);
	}
	if (req.query.white) {
		white = parseInt(req.query.white);
	}
	if (req.query.forceColors) {
		config.forceColors = parseBoolean(req.query.forceColors);
	}
	if (req.query.cloudLoggingInterval) {
		config.cloudLoggingInterval = parseInt(req.query.cloudLoggingInterval);
	}
	if (req.query.heatingMinTemperature) {
		config.heatingMinTemperature = parseFloat(req.query.heatingMinTemperature);
	}
	if (req.query.heatingMaxTemperature) {
		config.heatingMaxTemperature = parseFloat(req.query.heatingMaxTemperature);
	}
	for (const key of ['ca', 'mg', 'kh']) {
		if (req.query[`mineralPumps.${key}.time`]) {
			config.mineralsPumps[key].time = parseInt(req.query[`mineralPumps.${key}.time`]);
		}
		if (req.query[`mineralPumps.${key}.mL`]) {
			config.mineralsPumps[key].mL = parseInt(req.query[`mineralPumps.${key}.mL`]);
		}
	}
	for (let i = 0; i < 3; i++) {
		if (req.query[`phMeter.points.${i}.adc`]) {
			config.phMeter.points[i].adc = parseInt(req.query[`phMeter.points.${i}.adc`]);
		}
		if (req.query[`phMeter.points.${i}.pH`]) {
			config.phMeter.points[i].pH = parseFloat(req.query[`phMeter.points.${i}.pH`]);
		}
	}
	res.status(200);
	res.json(config);
});
app.get('/test', (req, res) => {
	res.status(204);
	res.json({});
});
app.get('/saveEEPROM', (req, res) => {
	res.sendStatus(200);
});
app.get('/getColorsCycle', (req, res) => {
	res.status(200);
	res.json(colorsCycle);
});
app.post('/setColorsCycle', (req, res) => {
	const matches = [...req.body.matchAll(/#(\d+),(\d+),(\d+),(\d+)@(\d+):(\d+)/g)];
	colorsCycle = matches.map(match => ({
		red:   parseInt(match[1]),
		green: parseInt(match[2]),
		blue:  parseInt(match[3]),
		white: parseInt(match[4]),
		time: `${match[5]}:${match[6]}`,
	}));
	res.sendStatus(200);
});
app.get('/resetColorsCycle', (req, res) => {
	colorsCycle = defaultColorCycle.map(e => Object.assign({}, e));
	res.sendStatus(200);
});

app.listen(8090);
