#pragma once

#include "common.hpp"
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

namespace CloudLogger {
	constexpr bool discardResponse = true;

	WiFiClientSecure sslClient;
	BearSSL::Session session;
	HTTPClient httpClient;

	inline unsigned long int getInterval() {
		return settings->cloud.interval;
	}
	inline void setInterval(unsigned long int ms) {
		settings->cloud.interval = ms;
	}

	inline bool isEnabled() {
		return getInterval() != 0 && WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED;
	}

#if CLOUDLOGGER_SECURE
	// GTS Root R1
	const char digicert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFYjCCBEqgAwIBAgIQd70NbNs2+RrqIQ/E8FjTDTANBgkqhkiG9w0BAQsFADBX
MQswCQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UE
CxMHUm9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTIwMDYx
OTAwMDA0MloXDTI4MDEyODAwMDA0MlowRzELMAkGA1UEBhMCVVMxIjAgBgNVBAoT
GUdvb2dsZSBUcnVzdCBTZXJ2aWNlcyBMTEMxFDASBgNVBAMTC0dUUyBSb290IFIx
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEAthECix7joXebO9y/lD63
ladAPKH9gvl9MgaCcfb2jH/76Nu8ai6Xl6OMS/kr9rH5zoQdsfnFl97vufKj6bwS
iV6nqlKr+CMny6SxnGPb15l+8Ape62im9MZaRw1NEDPjTrETo8gYbEvs/AmQ351k
KSUjB6G00j0uYODP0gmHu81I8E3CwnqIiru6z1kZ1q+PsAewnjHxgsHA3y6mbWwZ
DrXYfiYaRQM9sHmklCitD38m5agI/pboPGiUU+6DOogrFZYJsuB6jC511pzrp1Zk
j5ZPaK49l8KEj8C8QMALXL32h7M1bKwYUH+E4EzNktMg6TO8UpmvMrUpsyUqtEj5
cuHKZPfmghCN6J3Cioj6OGaK/GP5Afl4/Xtcd/p2h/rs37EOeZVXtL0m79YB0esW
CruOC7XFxYpVq9Os6pFLKcwZpDIlTirxZUTQAs6qzkm06p98g7BAe+dDq6dso499
iYH6TKX/1Y7DzkvgtdizjkXPdsDtQCv9Uw+wp9U7DbGKogPeMa3Md+pvez7W35Ei
Eua++tgy/BBjFFFy3l3WFpO9KWgz7zpm7AeKJt8T11dleCfeXkkUAKIAf5qoIbap
sZWwpbkNFhHax2xIPEDgfg1azVY80ZcFuctL7TlLnMQ/0lUTbiSw1nH69MG6zO0b
9f6BQdgAmD06yK56mDcYBZUCAwEAAaOCATgwggE0MA4GA1UdDwEB/wQEAwIBhjAP
BgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTkrysmcRorSCeFL1JmLO/wiRNxPjAf
BgNVHSMEGDAWgBRge2YaRQ2XyolQL30EzTSo//z9SzBgBggrBgEFBQcBAQRUMFIw
JQYIKwYBBQUHMAGGGWh0dHA6Ly9vY3NwLnBraS5nb29nL2dzcjEwKQYIKwYBBQUH
MAKGHWh0dHA6Ly9wa2kuZ29vZy9nc3IxL2dzcjEuY3J0MDIGA1UdHwQrMCkwJ6Al
oCOGIWh0dHA6Ly9jcmwucGtpLmdvb2cvZ3NyMS9nc3IxLmNybDA7BgNVHSAENDAy
MAgGBmeBDAECATAIBgZngQwBAgIwDQYLKwYBBAHWeQIFAwIwDQYLKwYBBAHWeQIF
AwMwDQYJKoZIhvcNAQELBQADggEBADSkHrEoo9C0dhemMXoh6dFSPsjbdBZBiLg9
NR3t5P+T4Vxfq7vqfM/b5A3Ri1fyJm9bvhdGaJQ3b2t6yMAYN/olUazsaL+yyEn9
WprKASOshIArAoyZl+tJaox118fessmXn1hIVw41oeQa1v1vg4Fv74zPl6/AhSrw
9U5pCZEt4Wi4wStz6dTZ/CLANx8LZh1J7QJVj2fhMtfTJr9w4z30Z209fOU0iOMy
+qduBmpvvYuR7hZL6Dupszfnw0Skfths18dG9ZKb59UhvmaSGZRVbNQpsg3BZlvi
d0lIKO2d1xozclOzgjXPYovJJIultzkMu34qQb9Sz/yilrbCgj8=
-----END CERTIFICATE-----
)EOF";

	BearSSL::X509List cert(digicert);
#endif

	struct Entry {
		float waterTemperature;
		float rtcTemperature;
		float phLevel;
	};

	void push(Entry entry) {
		do {
			[[maybe_unused]]
			unsigned long int startTime = millis();

			// Prepare HTTPS client
			httpClient.begin(sslClient, F("https://script.google.com/macros/s/AKfycbzNjb9VDXi9RayxOderzEnBSd-2OY8O6I-WqUD0YJTDE5wRQWC0/exec"));

			// Prepare request body
			DateTime now = rtc.now();
			constexpr unsigned int bufferLength = 400;
			char buffer[bufferLength];
			int writtenLength = snprintf(
				buffer, bufferLength,
				"{"
					"\"rtcTemperature\":%.2f,"
					"\"waterTemperature\":%.2f,"
					"\"phLevel\":%.6f,"
					"\"uptime\":%lu,"
					"\"heap\":{"
						"\"free\":%u,"
						"\"frag\":%u"
					"},"
					"\"secret\":\"%s\","
					"\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\""
				"}",
				entry.rtcTemperature,
				entry.waterTemperature,
				entry.phLevel,
				millis(), // will roll-over to 0 after 40+ days, but it's good enough.
				ESP.getFreeHeap(),
				ESP.getHeapFragmentation(),
				settings->cloud.secret,
				// Format should be like: "2004-02-12T15:19:21" (without time zones)
				now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second()
			);
			if (writtenLength < 0 || static_cast<unsigned int>(writtenLength) >= bufferLength) {
				LOG_ERROR(CloudLogger, "snprintf failed");
				break;
			}

			LOG_DEBUG(CloudLogger, "Logging sample to cloud: %s", buffer);

			// Send POST request
			httpClient.addHeader(F("Content-Type"), WEB_CONTENT_TYPE_APPLICATION_JSON);
			int statusCode = httpClient.POST(reinterpret_cast<uint8_t*>(buffer), writtenLength);
			if (statusCode <= 0 || 400 <= statusCode) {
				LOG_ERROR(CloudLogger, "Invalid status code: %i", statusCode);
				break;
			}

			if (!discardResponse) {
				// Read response
				const String& payload = httpClient.getString();
				LOG_TRACE(CloudLogger, "Response body: %s", payload.c_str());

				int offset = payload.indexOf(F("samplesCount\":"));
				if (offset == -1) {
					LOG_WARN(CloudLogger, "No or invalid response");
				}
				else {
					int samplesCount = atoi(payload.c_str() + offset);
					LOG_INFO(CloudLogger, "Total %i samples collected", samplesCount);
				}
			}

			LOG_DEBUG(CloudLogger, "Time used %lums", millis() - startTime);
		}
		while (0);

		// Disconnect
		httpClient.end();
	}

	void setup() {
		if (getInterval() != 0 && getInterval() < 10000) {
			LOG_WARN(CloudLogger, "Interval too short, using 10 seconds.");
			setInterval(10000);
		}

		// Prepare SSL client
		sslClient.setSession(&session);
#if CLOUDLOGGER_SECURE
		sslClient.setTrustAnchors(&cert);
		sslClient.setX509Time(rtc.now().unixtime());
#else
		sslClient.setInsecure();
		sslClient.setCiphersLessSecure();
#endif

		// Prepare HTTP client
		httpClient.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
		httpClient.setTimeout(timeoutForCloudLogger);
	}
}
