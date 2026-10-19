#include "CoolerBox.h"

namespace {
const char LINK_LOG_PATH[] = "/wifi-link.log";
const uint8_t LINK_LOG_BUFFER_SIZE = 8;
const size_t LINK_LOG_MESSAGE_SIZE = 112;
const size_t LINK_LOG_LINE_SIZE = 144;
const size_t LINK_LOG_MAX_SIZE = 500UL * 1024UL;

struct BufferedLinkLogLine {
	char message[LINK_LOG_MESSAGE_SIZE];
	uint32_t millisAtEvent;
};

struct LinkLogCounters {
	uint32_t uploadSuccesses = 0;
	uint32_t uploadFailures = 0;
	uint32_t staDisconnects = 0;
	uint32_t apDisconnects = 0;
};

LinkLogCounters counters;
BufferedLinkLogLine linkLogBuffer[LINK_LOG_BUFFER_SIZE] = {};
uint8_t bufferedLines = 0;
uint32_t droppedLogRecords = 0;
unsigned long lastWriteErrorLogMillis = 0;
bool writeErrorLogged = false;

void formatUptime(char *buffer, size_t bufferSize, uint32_t eventMillis) {
	unsigned long seconds =
		(static_cast<uint32_t>(millis()) - eventMillis) / 1000U;
	unsigned long days = seconds / 86400UL;
	unsigned long hours = (seconds / 3600UL) % 24UL;
	unsigned long minutes = (seconds / 60UL) % 60UL;
	seconds %= 60UL;
	if (days > 0) {
		snprintf(buffer, bufferSize, "%lud %02lu:%02lu:%02lu",
		         static_cast<unsigned long>(days), hours, minutes, seconds);
	} else {
		snprintf(buffer, bufferSize, "%02lu:%02lu:%02lu", hours, minutes, seconds);
	}
}

size_t formatLinkLogLine(char *buffer, size_t bufferSize, const BufferedLinkLogLine &line) {
	char timestamp[24];
	time_t now = time(nullptr);
	if (now >= 1700000000) {
		uint32_t elapsedMillis = static_cast<uint32_t>(millis()) - line.millisAtEvent;
		time_t eventTime = now - elapsedMillis / 1000U;
		struct tm localTime;
		if (localtime_r(&eventTime, &localTime) &&
		    strftime(timestamp, sizeof(timestamp), "[%Y-%m-%d %H:%M:%S]", &localTime) > 0) {
			return snprintf(buffer, bufferSize, "%s %s\n", timestamp, line.message);
		}
	}

	formatUptime(timestamp, sizeof(timestamp), line.millisAtEvent);
	return snprintf(buffer, bufferSize, "[uptime %s] %s\n", timestamp, line.message);
}

bool flushLogBuffer() {
	if (bufferedLines == 0) {
		return true;
	}

	size_t pendingBytes = 0;
	for (uint8_t i = 0; i < bufferedLines; ++i) {
		char formattedLine[LINK_LOG_LINE_SIZE];
		pendingBytes += formatLinkLogLine(formattedLine, sizeof(formattedLine), linkLogBuffer[i]);
	}
	size_t existingSize = 0;
	if (LittleFS.exists(LINK_LOG_PATH)) {
		File existingFile = LittleFS.open(LINK_LOG_PATH, "r");
		if (existingFile) {
			existingSize = existingFile.size();
			existingFile.close();
		}
	}
	if (existingSize > LINK_LOG_MAX_SIZE || pendingBytes > LINK_LOG_MAX_SIZE - existingSize) {
		if (!LittleFS.remove(LINK_LOG_PATH)) {
			Debug.addLine(F("Failed to purge oversized Wi-Fi link log"));
			return false;
		}
		Debug.addLine(F("Purged Wi-Fi link log after exceeding 500 KB"));
	}

	File file = LittleFS.open(LINK_LOG_PATH, "a");
	if (!file) {
		if (!writeErrorLogged ||
		    static_cast<unsigned long>(millis() - lastWriteErrorLogMillis) >= 60000UL) {
			Debug.addLine(F("Failed to open Wi-Fi link log in LittleFS"));
			lastWriteErrorLogMillis = millis();
			writeErrorLogged = true;
		}
		return false;
	}

	bool complete = true;
	for (uint8_t i = 0; i < bufferedLines; ++i) {
		char formattedLine[LINK_LOG_LINE_SIZE];
		size_t lineLength = formatLinkLogLine(formattedLine, sizeof(formattedLine), linkLogBuffer[i]);
		if (file.write(reinterpret_cast<const uint8_t *>(formattedLine), lineLength) != lineLength) {
			complete = false;
			break;
		}
	}
	file.flush();
	file.close();
	if (!complete) {
		if (!writeErrorLogged ||
		    static_cast<unsigned long>(millis() - lastWriteErrorLogMillis) >= 60000UL) {
			Debug.addLine(F("Failed to write Wi-Fi link log to LittleFS"));
			lastWriteErrorLogMillis = millis();
			writeErrorLogged = true;
		}
		return false;
	}

	bufferedLines = 0;
	writeErrorLogged = false;
	return true;
}

const char *eventName(LinkDiagnosticEvent event) {
	switch (event) {
	case LinkDiagnosticEvent::UploadSucceeded: return "upload_ok";
	case LinkDiagnosticEvent::UploadFailed: return "upload_failed";
	case LinkDiagnosticEvent::StaDisconnected: return "sta_disconnected";
	case LinkDiagnosticEvent::ApDisconnected: return "ap_disconnected";
	}
	return "unknown";
}

void appendSummary(String &output, const char *label, uint32_t value) {
	output += label;
	output += value;
	output += '\n';
}
}

void recordLinkDiagnostic(LinkDiagnosticEvent event, uint8_t backoffMinutes) {
	switch (event) {
	case LinkDiagnosticEvent::UploadSucceeded:
		++counters.uploadSuccesses;
		break;
	case LinkDiagnosticEvent::UploadFailed:
		++counters.uploadFailures;
		break;
	case LinkDiagnosticEvent::StaDisconnected:
		++counters.staDisconnects;
		break;
	case LinkDiagnosticEvent::ApDisconnected:
		++counters.apDisconnects;
		break;
	}

	if (bufferedLines >= LINK_LOG_BUFFER_SIZE && !flushLogBuffer()) {
		++droppedLogRecords;
		return;
	}

	BufferedLinkLogLine &line = linkLogBuffer[bufferedLines];
	line.millisAtEvent = static_cast<uint32_t>(millis());
	snprintf(line.message, sizeof(line.message),
	         "event=%s success=%lu failed=%lu sta_disc=%lu ap_disc=%lu backoff=%um",
	         eventName(event),
	         static_cast<unsigned long>(counters.uploadSuccesses),
	         static_cast<unsigned long>(counters.uploadFailures),
	         static_cast<unsigned long>(counters.staDisconnects),
	         static_cast<unsigned long>(counters.apDisconnects),
	         backoffMinutes);
	++bufferedLines;

	if (bufferedLines == LINK_LOG_BUFFER_SIZE) {
		flushLogBuffer();
	}
}

void handleLinkLog() {
	String summary;
	summary.reserve(192);
	summary = F("Current boot totals:\n");
	appendSummary(summary, "Successful uploads: ", counters.uploadSuccesses);
	appendSummary(summary, "Failed uploads: ", counters.uploadFailures);
	appendSummary(summary, "STA disconnects: ", counters.staDisconnects);
	appendSummary(summary, "AP disconnects: ", counters.apDisconnects);
	summary += F("Current backoff: ");
	summary += telemetryBackoffLevelMinutes();
	summary += F(" minutes\n");
	if (droppedLogRecords != 0) {
		appendSummary(summary, "Log records lost after LittleFS write failures: ", droppedLogRecords);
	}
	summary += F("\nPersisted and buffered event history:\n");

	httpServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
	httpServer.send(200, "text/plain; charset=utf-8", summary);

	File file = LittleFS.open(LINK_LOG_PATH, "r");
	if (file) {
		char chunk[128];
		while (file.available()) {
			size_t count = file.readBytes(chunk, sizeof(chunk));
			if (count > 0) {
				httpServer.sendContent(chunk, count);
			}
			yield();
		}
		file.close();
	} else {
		httpServer.sendContent(F("No persisted link events yet.\n"));
	}
	for (uint8_t i = 0; i < bufferedLines; ++i) {
		char formattedLine[LINK_LOG_LINE_SIZE];
		size_t lineLength = formatLinkLogLine(formattedLine, sizeof(formattedLine), linkLogBuffer[i]);
		httpServer.sendContent(formattedLine, lineLength);
	}
	httpServer.sendContent("");
}
