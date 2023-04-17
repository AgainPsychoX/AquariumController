
import 'google-apps-script';

const sheetID = '1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o';
const secret = 'jacek';

const sampleValueNames = ['rtcTemperature', 'waterTemperature', 'phLevel'] as const;
type SampleValueName = typeof sampleValueNames[number];
type SampleValues = { [key in SampleValueName]: number; }
type Sample = { timestamp: Date; } & SampleValues;

type SampleValueSummary = {
	min: number,
	max: number,
	avg: number,
	mean: number,
};
type SampleValueSummaries = {
	[key in SampleValueName]: SampleValueSummary;
}
interface SummaryRowData extends SampleValueSummaries {
	dateAndHour: Date;
	rawDataRow: number;
	samplesCount: number;
};
class SummaryRow implements SummaryRowData {
	dateAndHour: Date;
	rawDataRow: number;
	samplesCount: number;
	rtcTemperature: SampleValueSummary;
	waterTemperature: SampleValueSummary;
	phLevel: SampleValueSummary;

	constructor(data: SummaryRowData) {
		if (this == data) return;
		Object.assign(this, data);
	}

	static parse(v: any[][]): SummaryRow {
		const r = v[0];
		return new SummaryRow({
			dateAndHour: r[0] as Date,
			rawDataRow: r[1],
			samplesCount: r[2],
			rtcTemperature:   { min: r[3],  max: r[4],  avg: r[5],  mean: r[6]  },
			waterTemperature: { min: r[7],  max: r[8],  avg: r[9],  mean: r[10] },
			phLevel:          { min: r[11], max: r[12], avg: r[13], mean: r[14] },
		});
	}
}
class SummaryRowHandle extends SummaryRow {
	range: GoogleAppsScript.Spreadsheet.Range;
	isLast: boolean;

	constructor(range: GoogleAppsScript.Spreadsheet.Range, data: SummaryRowData) {
		super(data);
		this.range = range;
		this.isLast = false;
	}

	static fromRange(range: GoogleAppsScript.Spreadsheet.Range) {
		return new SummaryRowHandle(range, SummaryRow.parse(range.getValues()));
	}
	
	commit() {
		this.range.setValues([[
			this.dateAndHour,
			this.rawDataRow,
			this.samplesCount,
			this.rtcTemperature.min,
			this.rtcTemperature.max,
			this.rtcTemperature.avg,
			this.rtcTemperature.mean,
			this.waterTemperature.min,
			this.waterTemperature.max,
			this.waterTemperature.avg,
			this.waterTemperature.mean,
			this.phLevel.min,
			this.phLevel.max,
			this.phLevel.avg,
			this.phLevel.mean,
		]]);
		this.range.getCell(1, 1).setNumberFormat("yyyy-mm-dd hh:mm");
	}
}

class MyRepository {
	spreadsheet: GoogleAppsScript.Spreadsheet.Spreadsheet;
	samplesSummarySheet: GoogleAppsScript.Spreadsheet.Sheet;

	constructor(sheetID) {
		this.spreadsheet = SpreadsheetApp.openById(sheetID);
		this.samplesSummarySheet = this._getSamplesSummarySheet();
	}

	_createSheetForSamples(name: string) {
		const templateSheetName = `Template: Samples monthly`;
		const template = this.spreadsheet.getSheetByName(templateSheetName);
		if (!template) throw new Error(`Cannot find '${templateSheetName}' sheet`);
		const sheet = template.copyTo(this.spreadsheet);
		sheet.setName(name);
		return sheet;
	}

	_getSheetForSamples(timestamp: Date) {
		const name = `Samples ${new Date(+timestamp + 1000 * 60 * 30).toISOString().substring(0, 7)}`
		return this.spreadsheet.getSheetByName(name);
	}

	_getOrCreateSheetForSamples(timestamp: Date) {
		const name = `Samples ${new Date(+timestamp + 1000 * 60 * 30).toISOString().substring(0, 7)}`
		const sheet = this.spreadsheet.getSheetByName(name);
		if (!sheet)
			return { sheet: this._createSheetForSamples(name), fresh: true };
		else
			return { sheet, fresh: false };
	}

	_getSamplesSummarySheet() {
		const summarySheetName = `Samples summary`;
		const sheet = this.spreadsheet.getSheetByName(summarySheetName);
		if (!sheet) throw new Error(`Cannot find '${summarySheetName}' sheet`);
		return sheet;
	}

	_floorDateToHourCenteredBin(timestamp: Date) {
		const a = new Date(+timestamp + 1000 * 60 * 30);
		return new Date(+a - +a % (1000 * 60 * 60));
	}

	_getSummaryRowHandle(timestamp: Date) {
		const sheet = this.samplesSummarySheet;
		const rows = sheet.getRange(2, 1, sheet.getLastRow() - 1, 1).getValues().map(r => r[0] as Date);
		const index = rows.findIndex(r => new Date(+r - 1000 * 60 * 30) <= timestamp && timestamp < new Date(+r + 1000 * 60 * 30));
		if (index == -1) {
			sheet.appendRow([
				this._floorDateToHourCenteredBin(timestamp), // floor date to hour mark
				0, 0 // row & count zero to determine fresh summary rows
			]);
			return SummaryRowHandle.fromRange(sheet.getRange(sheet.getLastRow(), 1, 1, 15));
		}
		else {
			return SummaryRowHandle.fromRange(sheet.getRange(2 + index, 1, 1, 15));
		}
	}

	pushSample(sample: Sample) {
		if (sample.phLevel < 3 || 12 < sample.phLevel || sample.waterTemperature == 0) {
			// Ignore obviously invalid samples (disconnected sensors)
			return;
		}

		const summaryRow = this._getSummaryRowHandle(sample.timestamp);
		const { sheet: rawDataSheet, fresh: freshRawDataSheet } = this._getOrCreateSheetForSamples(sample.timestamp);

		// Add row with data
		if (freshRawDataSheet) {
			summaryRow.rawDataRow = rawDataSheet.getLastRow();
		}
		else {
			if (summaryRow.samplesCount == 0) {
				summaryRow.rawDataRow = rawDataSheet.getLastRow() + 1;
			}
			rawDataSheet.insertRowAfter(summaryRow.rawDataRow + summaryRow.samplesCount - 1); // will also copy formatting
		}
		rawDataSheet.getRange(summaryRow.rawDataRow + summaryRow.samplesCount, 1, 1, 4).setValues([[
			sample.timestamp,
			sample.rtcTemperature,
			sample.waterTemperature,
			sample.phLevel,
		]]);
		summaryRow.samplesCount += 1;

		// Update raw data indexes of summary entries that come after current one
		if (summaryRow.range.getRow() < this.samplesSummarySheet.getLastRow()) {
			const range = this.samplesSummarySheet.getRange(summaryRow.range.getRow() + 1, 1, Infinity, 2);
			const values = range.getValues();
			for (const row of values) {
				row[1] += 1;
			}
			range.setValues(values);
		}

		// Update summary row
		const samples = rawDataSheet.getSheetValues(summaryRow.rawDataRow, 1, summaryRow.samplesCount, 4)
			.map(r => ({
				timestamp: new Date(r[0]),
				rtcTemperature: parseFloat(r[1]),
				waterTemperature: parseFloat(r[2]),
				phLevel: parseFloat(r[3]),
			} as Sample))
		;
		for (const key of sampleValueNames) {
			const array = samples.map(s => s[key]).sort();
			summaryRow[key].min = array[0];
			summaryRow[key].max = array[array.length - 1];
			const sum = array.reduce((p, c) => p + c, 0);
			summaryRow[key].avg = sum / array.length;
			const middle = array.length / 2;
			summaryRow[key].mean = (middle % 1 == 0) 
				? (array[middle - 1] + array[middle]) / 2
				: array[Math.floor(middle)];
		}
		summaryRow.commit();
	}
}

/** 
 * Handles posts requests, as reporting readings from the controller to the sheet.
 * 
 * Example data:
 * ```
 * {
 *    "rtcTemperature":28.1,
 *    "waterTemperature":27.3,
 *    "phLevel":7.11,
 *    "uptime": 612345,
 *    "heap": {
 *        "free": 1234,
 *        "frag": 54
 *    },
 *    "secret": "jacek",
 *    "timestamp":"2020-02-23T16:17:18"
 * }
 * ```
 */
function doPost(request: GoogleAppsScript.Events.DoPost) {
	const repository = new MyRepository(sheetID);

	try {
		const data = JSON.parse(request.postData.contents);

		if (secret != data['secret']) {
			throw new Error(`Invalid secret: ${data['secret']}`);
		}

		const requestTimestamp = new Date();
		const uptime = parseInt(data['uptime']); // ms

		const sample: Sample = {
			timestamp: new Date(data['timestamp']),
			rtcTemperature: parseFloat(data['rtcTemperature']),
			waterTemperature: parseFloat(data['waterTemperature']),
			phLevel: parseFloat(data['phLevel']),
		};
		const heapInfo = data['heap'];

		repository.pushSample(sample);
		// TODO: add events
		// TODO: add grouping samples & events
		// TODO: rewrite response

		// For now, old code is used as well
		// #region OLD
		const spreadsheet = SpreadsheetApp.openById(sheetID);
		const sheet = spreadsheet.getSheetByName('RawData');
		if (!sheet) {
			throw new Error(`Couldn't find sheet!`);
		}

		const isInvalid = sample.phLevel < 3 || 12 < sample.phLevel || sample.waterTemperature == 0;

		if (!isInvalid) {
			sheet.appendRow([
				requestTimestamp,
				sample.timestamp,
				sample.rtcTemperature,
				sample.waterTemperature,
				sample.phLevel,
				uptime,
				heapInfo.free,
				heapInfo.frag,
			]);
		}

		const lastRow = sheet.getLastRow();
		const response: any = {
			samplesCount: lastRow - 1,
		};

		if (isInvalid) {
			console.warn("Invalid data (probably disconnected sensors), omitting.");
			response.warn = 'invalid';
		}
		// #endregion

		return ContentService.createTextOutput(JSON.stringify(response)).setMimeType(ContentService.MimeType.JSON);
	}
	catch (error) {
		console.error(error, error.stack, {request: JSON.stringify(request)});
		
		const response = {
			error: error.toString(),
			stack: error.stack,
		};
		return ContentService.createTextOutput(JSON.stringify(response)).setMimeType(ContentService.MimeType.JSON);
	}
}
