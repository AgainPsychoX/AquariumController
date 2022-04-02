
const { program } = require('commander');
const packageJSON = require('./package.json');

program
	.name('prepareWebArduino')
	.version(packageJSON.version)
	.description('Prepares static web content for easy including in Arduino project. Uses PROGMEM where possible.')
	.option('-i, --input-directory [directory]',    'specify input directory',                       'web/')
	.option('-o, --output-directory [directory]',   'specify output directory',                      'webEncoded/')
	.option('--clean',                              'removes everything from output directory first')
	.option('--include-all-basename [basename]',    'change name of include-all file',               'WebStaticContent')
	.option('--common-code-basename [basename]',    'change basename of common code file',           'WebCommonUtils')
	.option('--no-timestamp',                       'disable adding timestamp to include-all file')
	.option('--no-debug-prints',                    'disable debug printing (on every static content served)')
	.option('--debug-print-snippet [code]',         'code for debug printing',                       'Serial.println("Web serving static /${path}")')
	.option('--debug',                              'enable debug logging level')
	.action(async function(options) {
		if (!options.debug) {
			console.debug = () => {};
		}

		const totalTimeTaken = `Total time`;
		console.time(totalTimeTaken);
		
		require('require-json5').replace();
		const fs = require('fs-extra');
		const path = require('path/posix');
		const zlib = require('zlib');

		const data = require('./data.json');

		if (options.clean) {
			console.log(`Cleaning ${options.outputDirectory}`);
			await fs.emptyDir(options.outputDirectory);
		}

		const aboutMeMessage = (`
// Generated `) + (!options.timestamp ? '' : (`at ${new Date().toString()}
// `)) + (`using 'prepareWebArduino.js' v${packageJSON.version} by ${packageJSON.author}
`);

		const onlyUnique = (value, index, self) => self.indexOf(value) === index;

		const getExtension = (path) => {
			// from https://stackoverflow.com/a/12900504/4880243
			const basename = path.split(/[\\/]/).pop();	// extract file name from full path ...
														// (supports `\\` and `/` separators)
			const pos = basename.lastIndexOf('.');		// get last position of `.`
			if (basename === '' || pos < 1) {			// if file name is empty or ...
				return '';								//  `.` not found (-1) or comes first (0)
			}
			return basename.slice(pos + 1);				// extract extension ignoring `.`
		};

		const getConstNameForMimeType = (mimeType) => `WEB_CONTENT_TYPE_${mimeType.replace(/[./+-]/g, '_').toUpperCase()}`;
		const getConstNameForPathPart = (path) => path.replace(/[./\-+#\(\)\[\]]/g, '_');

		// Processing files
		const processed = [];
		const processFile = async (filename, inputPath, relativePath) => {
			console.debug(`Processing ${inputPath}`);
			const outputPath = path.join(options.outputDirectory, filename + '.cpp');
			const data = await fs.readFile(inputPath);
			let hexString = zlib.gzipSync(data).toString('hex');
			const hexDigitPairs = [];
			while (hexString.length) {
				hexDigitPairs.push(hexString.substring(0, 2));
				hexString = hexString.substring(2);
			}
			const constNamePathPart = getConstNameForPathPart(relativePath);
			await fs.writeFile(outputPath, (`#include <pgmspace.h>

extern const char WEB_${constNamePathPart}_CONTENT[] PROGMEM;
extern const char WEB_${constNamePathPart}_PATH[] PROGMEM;

const char WEB_${constNamePathPart}_PATH[] PROGMEM = "/${relativePath}";
const char WEB_${constNamePathPart}_CONTENT[] PROGMEM = {
${'0x' + hexDigitPairs.join(', 0x')}
};
`
			));
			const rawSize = data.length;
			const compressedSize = hexDigitPairs.length;
			console.log(`Processed ${inputPath.padEnd(32)} \tRaw size: ${rawSize.toString().padStart(6)}\tCompressed: ${compressedSize.toString().padStart(6)}`)
			processed.push({
				relativePath,
				rawSize,
				compressedSize,
			});
		}
		const processDirectory = async (directoryPath, directoryRelativePath) => {
			const files = await fs.opendir(directoryPath);
			const promises = [];
			for await (const file of files) {
				const relativePath = path.join(directoryRelativePath, file.name);
				const inputPath = path.join(options.inputDirectory, file.name);
				if (file.isDirectory()) {
					console.log(`Processing subdirectory ${inputPath}`);
					promises.push(processDirectory(inputPath, relativePath));
				}
				else if (file.isFile()) {
					promises.push(processFile(file.name, inputPath, relativePath))
				}
			}
			await Promise.all(promises);
		}
		await processDirectory(options.inputDirectory, '');

		// Compilation unit for common code
		const padding1 = Math.ceil((
			'const char WEB_CONTENT_TYPE_[] PROGMEM '.length +
			Object.values(data.mimeTypes)
				.map(str => str.length)
				.reduce((prev, curr) => Math.max(prev, curr))
		) / 4) * 4;
		console.log(`Adding common code source file: ${options.commonCodeBasename}.cpp`);
		await fs.writeFile(
			path.join(options.outputDirectory, options.commonCodeBasename + '.cpp'),
			(`#include "${options.commonCodeBasename}.hpp"

const char WEB_CACHE_CONTROL_P[] PROGMEM            = "Cache-Control";
const char WEB_CACHE_CONTROL_CACHE_P[] PROGMEM      = "max-age=315360000, public, immutable";
const char WEB_CONTENT_ENCODING_P[] PROGMEM         = "Content-Encoding";
const char WEB_CONTENT_ENCODING_GZIP_P[] PROGMEM    = "gzip";

const String WEB_CACHE_CONTROL          = String(FPSTR(WEB_CACHE_CONTROL_P));
const String WEB_CACHE_CONTROL_CACHE    = String(FPSTR(WEB_CACHE_CONTROL_CACHE_P));
const String WEB_CONTENT_ENCODING       = String(FPSTR(WEB_CONTENT_ENCODING_P));
const String WEB_CONTENT_ENCODING_GZIP  = String(FPSTR(WEB_CONTENT_ENCODING_GZIP_P));

`
			) + 
			Object.values(data.mimeTypes)
				.filter(onlyUnique)	
				.map(mimeType => 
					`const char ${getConstNameForMimeType(mimeType)}[] PROGMEM `.padEnd(padding1) +
					`= "${mimeType}";`
				)
				.join('\n')
			+
			'\n'
		);

		// Common code header
		console.log(`Adding common code header file ${options.commonCodeBasename}.hpp`);
		await fs.writeFile(
			path.join(options.outputDirectory, options.commonCodeBasename + '.hpp'),
			(
				(`#pragma once
#include <Arduino.h>

extern const String WEB_CACHE_CONTROL;
extern const String WEB_CACHE_CONTROL_CACHE;
extern const String WEB_CONTENT_ENCODING;
extern const String WEB_CONTENT_ENCODING_GZIP;

#define WEB_USE_CACHE_STATIC(server) server.sendHeader(WEB_CACHE_CONTROL,    WEB_CACHE_CONTROL_CACHE)
#define WEB_USE_GZIP_STATIC(server)  server.sendHeader(WEB_CONTENT_ENCODING, WEB_CONTENT_ENCODING_GZIP)

`
				) + 
				Object.values(data.mimeTypes)
					.filter(onlyUnique)
					.map(mimeType => 
						`extern const char ${getConstNameForMimeType(mimeType)}[] PROGMEM;`
					)
					.join('\n')
				+
				'\n'
			)
		);

		// Preparing include-all header
		console.log(`Adding include all header file: ${options.includeAllBasename}.hpp`);
		await fs.writeFile(
			path.join(options.outputDirectory, options.includeAllBasename + '.hpp'),
			(
				(`#pragma once
${aboutMeMessage}
#include <Arduino.h>
#include "${options.commonCodeBasename}.hpp"

// Prepared content values externs`
				) +
				processed.sort((a, b) => a.relativePath.localeCompare(b.relativePath)).map(entry => {
					const path = entry.relativePath;
					const constNamePathPart = getConstNameForPathPart(path);
					return (`
extern const char WEB_${constNamePathPart}_PATH[] PROGMEM;
extern const char WEB_${constNamePathPart}_CONTENT[] PROGMEM;
constexpr unsigned short int WEB_${constNamePathPart}_CONTENT_LENGTH = ${entry.compressedSize};`
					);
				}).join('') + 
				(`

// Macro to setup everything
#define WEB_REGISTER_ALL_STATIC(server) do { \\`
				) + 
				processed.map(entry => {
					const path = entry.relativePath;
					const constNamePathPart = getConstNameForPathPart(path);
					const mimeType = data.mimeTypes[getExtension(path).toLowerCase()] || data.mimeTypes['default'];
					return (`
	server.on(WEB_${constNamePathPart}_PATH, []() { \\`
					) + (!options.debugPrints ? '' : (`
		${options.debugPrintSnippet.replace('${path}', path)}; \\`
					)) + (`
		WEB_USE_CACHE_STATIC(server); \\
		WEB_USE_GZIP_STATIC(server); \\
		server.send_P(200, ${getConstNameForMimeType(mimeType)}, WEB_${constNamePathPart}_CONTENT, WEB_${constNamePathPart}_CONTENT_LENGTH); \\
	}); \\`
					);
				}).join('') + (
`
} while (0)
`
				)
			)
		);

		console.timeEnd(totalTimeTaken);
		console.log('Done!');
	})
;

program.on('--help', () => {
	console.log('');
	console.log(`Created by Patryk 'PsychoX' Ludwikowski <psychoxivi+embedded@gmail.com>`);
});

program.parse(process.argv);
