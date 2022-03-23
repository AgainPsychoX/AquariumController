
// Generated using 'prepareWebArduino.js' by Patryk 'PsychoX' Ludwikowski <psychoxivi+arduino@gmail.com>

#include "charts.js.hpp"
#include "index.html.hpp"
#include "main.js.hpp"
#include "style.css.hpp"
#include "wykres.html.hpp"

const char WEB_CACHE_CONTROL_P[] PROGMEM          = "Cache-Control";
const char WEB_CACHE_CONTROL_CACHE_P[] PROGMEM    = "max-age=315360000, public, immutable";
const char WEB_CONTENT_ENCODING_P[] PROGMEM       = "Content-Encoding";
const char WEB_CONTENT_ENCODING_GZIP_P[] PROGMEM  = "gzip";

const String WEB_CACHE_CONTROL         = String(FPSTR(WEB_CACHE_CONTROL_P));
const String WEB_CACHE_CONTROL_CACHE   = String(FPSTR(WEB_CACHE_CONTROL_CACHE_P));
const String WEB_CONTENT_ENCODING      = String(FPSTR(WEB_CONTENT_ENCODING_P));
const String WEB_CONTENT_ENCODING_GZIP = String(FPSTR(WEB_CONTENT_ENCODING_GZIP_P));

const char WEB_CONTENT_TYPE_TEXT_PLAIN[] PROGMEM                = "text/plain";
const char WEB_CONTENT_TYPE_TEXT_HTML[] PROGMEM                 = "text/html";
const char WEB_CONTENT_TYPE_TEXT_JAVASCRIPT[] PROGMEM           = "text/javascript";
const char WEB_CONTENT_TYPE_TEXT_CSS[] PROGMEM                  = "text/css";
const char WEB_CONTENT_TYPE_IMAGE_PNG[] PROGMEM                 = "image/png";
const char WEB_CONTENT_TYPE_IMAGE_X_ICON[] PROGMEM              = "image/x-icon";
const char WEB_CONTENT_TYPE_APPLICATION_JSON[] PROGMEM          = "application/json";
const char WEB_CONTENT_TYPE_APPLICATION_OCTET_STREAM[] PROGMEM  = "application/octet-stream";

#define WEB_USE_CACHE_STATIC(server) server.sendHeader(WEB_CACHE_CONTROL,    WEB_CACHE_CONTROL_CACHE)
#define WEB_USE_GZIP_STATIC(server)  server.sendHeader(WEB_CONTENT_ENCODING, WEB_CONTENT_ENCODING_GZIP)

#define WEB_REGISTER_ALL_STATIC(server) do { \
	server.on(F("/charts.js"), []() { \
		WEB_USE_CACHE_STATIC(server); \
		WEB_USE_GZIP_STATIC(server); \
		server.send_P(200, WEB_CONTENT_TYPE_TEXT_JAVASCRIPT, WEB_charts_js, sizeof(WEB_charts_js)); \
	}); \
	server.on(F("/index.html"), []() { \
		WEB_USE_CACHE_STATIC(server); \
		WEB_USE_GZIP_STATIC(server); \
		server.send_P(200, WEB_CONTENT_TYPE_TEXT_HTML, WEB_index_html, sizeof(WEB_index_html)); \
	}); \
	server.on(F("/main.js"), []() { \
		WEB_USE_CACHE_STATIC(server); \
		WEB_USE_GZIP_STATIC(server); \
		server.send_P(200, WEB_CONTENT_TYPE_TEXT_JAVASCRIPT, WEB_main_js, sizeof(WEB_main_js)); \
	}); \
	server.on(F("/style.css"), []() { \
		WEB_USE_CACHE_STATIC(server); \
		WEB_USE_GZIP_STATIC(server); \
		server.send_P(200, WEB_CONTENT_TYPE_TEXT_CSS, WEB_style_css, sizeof(WEB_style_css)); \
	}); \
	server.on(F("/wykres.html"), []() { \
		WEB_USE_CACHE_STATIC(server); \
		WEB_USE_GZIP_STATIC(server); \
		server.send_P(200, WEB_CONTENT_TYPE_TEXT_HTML, WEB_wykres_html, sizeof(WEB_wykres_html)); \
	}); \
} while (0)
