
const sheetID = '1OeXW_dhXnBcgqe8lflZT3rbdBoCy9qz3bURGQ95YH9o';
const secret = 'jacek';

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
function doPost(request) {
  const spreadsheet = SpreadsheetApp.openById(sheetID);
  const sheet = spreadsheet.getSheetByName('RawData');

  try {
    if (!request || !request.postData || request.postData.length < 2) {
      throw new Error(`No data sent!`);
    }
    const data = JSON.parse(request.postData.contents);

    if (secret != data['secret']) {
      throw new Error(`Invalid secret: ${data['secret']}`);
    }

    const requestTimestamp = new Date();
    const deviceTimestamp = new Date(data['timestamp']);
    const rtcTemperature = parseFloat(data['rtcTemperature']);
    const waterTemperature = parseFloat(data['waterTemperature']);
    const phLevel = parseFloat(data['phLevel']);
    const uptime = parseInt(data['uptime']); // ms
    const heapInfo = data['heap'];

    const isInvalid = phLevel < 3 || 12 < phLevel || waterTemperature == 0;

    if (!isInvalid) {
      sheet.appendRow([
        requestTimestamp,
        deviceTimestamp,
        rtcTemperature,
        waterTemperature,
        phLevel,
        uptime,
        heapInfo.free,
        heapInfo.frag,
      ]);
    }

    const lastRow = sheet.getLastRow();
    const response = {
      samplesCount: lastRow - 1,
    };

    if (isInvalid) {
      console.warn("Invalid data (probably disconnected), omitting.");
      response.warn = 'invalid';
    }

    return ContentService.createTextOutput(JSON.stringify(response)).setMimeType(ContentService.MimeType.JSON);
  }
  catch (error) {
    console.error(error, {request: JSON.stringify(request)});
    
    const response = {
      error: error.toString(),
      stack: error.stack,
    };
    return ContentService.createTextOutput(JSON.stringify(response)).setMimeType(ContentService.MimeType.JSON);
  }
}
