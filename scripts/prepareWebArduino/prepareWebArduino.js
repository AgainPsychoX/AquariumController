
const { program } = require('commander');

program
	.name('prepareWebArduino')
	.version('0.6.0')
	.description('Prepares static web content for easy including in Arduino project. Uses PROGMEM where possible.')
	.option('-i, --input-directory [directory]', 'specify input directory', 'web/')
	.option('-o, --output-directory [directory]', 'specify output directory', 'webEncoded/')
	.option('--include-all-filename [filename]', 'change name of include-all file', 'AllStaticContent.hpp')
	.option('--no-timestamp', 'disable adding timestamp to include-all file')
	.option('--no-debug-prints', 'disable printing to Serial on every static content served')
	.action(async function(options) {
		const fs = require('fs').promises;
		const path = require('path/posix');
		const zlib = require('zlib');

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

		const startedTime = new Date();

		// Processing files
		const allFilesRelativePaths = [];
		const processFile = async (filename, inputPath, relativePath) => {
			console.log(`Processing ${inputPath}`);
			const outputPath = path.join(options.outputDirectory, filename + '.hpp');
			const data = await fs.readFile(inputPath);
			let hexString = zlib.gzipSync(data).toString('hex');
			const hexDigitPairs = [];
			while (hexString.length) {
				hexDigitPairs.push(hexString.substring(0, 2));
				hexString = hexString.substring(2);
			}
			await fs.writeFile(outputPath, (
`
const char WEB_${relativePath.replace(/[./\-+#\(\)\[\]]/g, '_')}[] PROGMEM = {
${'0x' + hexDigitPairs.join(', 0x')}
};
`
			));
			allFilesRelativePaths.push(relativePath);
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

		// Preparing include-all file
		console.log(`Adding include all file: ${options.includeAllFilename}`)
		await fs.writeFile(
			path.join(options.outputDirectory, options.includeAllFilename),
			(
				(`
// Generated `) + (!options.timestamp ? '' : (`at ${new Date().toString()}
// `)) + (`using 'prepareWebArduino.js' by Patryk 'PsychoX' Ludwikowski <psychoxivi+arduino@gmail.com>
`
				) +
				allFilesRelativePaths.sort().map(path => (`
#include "${path}.hpp"`
				)).join('') + (`

const char WEB_CACHE_CONTROL_P[] PROGMEM          = "Cache-Control";
const char WEB_CACHE_CONTROL_CACHE_P[] PROGMEM    = "max-age=315360000, public, immutable";
const char WEB_CONTENT_ENCODING_P[] PROGMEM       = "Content-Encoding";
const char WEB_CONTENT_ENCODING_GZIP_P[] PROGMEM  = "gzip";

const String WEB_CACHE_CONTROL         = String(FPSTR(WEB_CACHE_CONTROL_P));
const String WEB_CACHE_CONTROL_CACHE   = String(FPSTR(WEB_CACHE_CONTROL_P));
const String WEB_CONTENT_ENCODING      = String(FPSTR(WEB_CACHE_CONTROL_P));
const String WEB_CONTENT_ENCODING_GZIP = String(FPSTR(WEB_CACHE_CONTROL_P));

const char WEB_CONTENT_TYPE_TEXT_PLAIN[] PROGMEM                = "text/plain";
const char WEB_CONTENT_TYPE_TEXT_HTML[] PROGMEM                 = "text/html";
const char WEB_CONTENT_TYPE_TEXT_JAVASCRIPT[] PROGMEM           = "text/javascript";
const char WEB_CONTENT_TYPE_TEXT_CSS[] PROGMEM                  = "text/css";
const char WEB_CONTENT_TYPE_IMAGE_PNG[] PROGMEM                 = "image/png";
const char WEB_CONTENT_TYPE_IMAGE_X_ICON[] PROGMEM              = "image/x-icon";
const char WEB_CONTENT_TYPE_APPLICATION_JSON[] PROGMEM          = "application/json";
const char WEB_CONTENT_TYPE_APPLICATION_OCTET_STREAM[] PROGMEM  = "application/octet-stream";

#define WEB_USE_CACHE_STATIC(server) \\
server.sendHeader(WEB_CACHE_CONTROL, WEB_CACHE_CONTROL_CACHE)
#define WEB_USE_GZIP_STATIC(server) \\
server.sendHeader(WEB_CONTENT_ENCODING, WEB_CONTENT_ENCODING_GZIP)

#define WEB_REGISTER_ALL_STATIC(server) do { \\`
				) + 
				allFilesRelativePaths.map(path => {
					const defName = path.replace(/[./\-+#\(\)\[\]]/g, '_');
					let mimeDef = ({
						'html': 'TEXT_HTML',
						'htm':  'TEXT_HTML',
						'js':   'TEXT_JAVASCRIPT',
						'css':  'TEXT_CSS',
						'png':  'IMAGE_PNG',
						'ico':  'IMAGE_X_ICON',
						'json': 'APPLICATION_JSON',
					})[getExtension(path).toLowerCase()] || 'APPLICATION_OCTET_STREAM';
					return (`
server.on(F("/${path}"), []() { \\`
					) + (!options.debugPrints ? '' : (`
Serial.println(F("[WEB] requesting: /${path}")); \\`
					)) + (`
WEB_USE_CACHE_STATIC(server); \\
WEB_USE_GZIP_STATIC(server); \\
server.send_P(200, WEB_CONTENT_TYPE_${mimeDef}, WEB_${defName}, sizeof(WEB_${defName})); \\
}); \\`
					);
				}).join('') + (
`
} while (0)
`
				)
			)
		);

		// Removing obstacle output files
		await Promise.all((await fs.readdir(options.outputDirectory)).map(async (filename) => {
			const outputPath = path.join(options.outputDirectory, filename);
			const { mtime } = await fs.stat(outputPath);
			if (mtime <= startedTime) {
				console.log(`Removing obstacle output file: ${outputPath}`);
				fs.unlink(outputPath);
			}
		}));

		console.log('Done!');
	})
;

program.on('--help', () => {
	console.log('');
	console.log(`Created by Patryk 'PsychoX' Ludwikowski <psychoxivi+arduino@gmail.com>`);
});

program.parse(process.argv);
