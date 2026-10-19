#include "CoolerBox.h"

namespace {
const char LINK_LOG_PATH[] = "/wifi-link.log";
const char LINK_LOG_OLD_PATH[] = "/wifi-link.old.log";
const uint8_t LINK_LOG_BUFFER_SIZE = 8;
const size_t LINK_LOG_MESSAGE_SIZE = 112;
const size_t LINK_LOG_LINE_SIZE = 144;
const size_t LINK_LOG_MAX_SIZE = 128UL * 1024UL;

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
size_t serializedBufferSize = 0;
bool bufferSerialized = false;
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

void logWriteError(const __FlashStringHelper *message) {
	if (!writeErrorLogged ||
	    static_cast<unsigned long>(millis() - lastWriteErrorLogMillis) >= 60000UL) {
		Debug.addLine(message);
		lastWriteErrorLogMillis = millis();
		writeErrorLogged = true;
	}
}

bool buildLogBatch() {
	size_t batchSize = 0;
	for (uint8_t i = 0; i < bufferedLines; ++i) {
		BufferedLinkLogLine line = linkLogBuffer[i];
		char formattedLine[LINK_LOG_LINE_SIZE];
		size_t lineSize = formatLinkLogLine(formattedLine, sizeof(formattedLine), line);
		if (lineSize > sizeof(BufferedLinkLogLine) ||
		    batchSize + lineSize > sizeof(linkLogBuffer)) {
			return false;
		}
		memcpy(reinterpret_cast<char *>(linkLogBuffer) + batchSize, formattedLine, lineSize);
		batchSize += lineSize;
	}
	serializedBufferSize = batchSize;
	return batchSize != 0;
}

bool flushLogBuffer() {
	if (bufferedLines == 0) {
		return true;
	}

	if (!bufferSerialized) {
		if (!buildLogBatch()) {
			logWriteError(F("Failed to format Wi-Fi link log batch"));
			return false;
		}
		bufferSerialized = true;
	}
	size_t pendingBytes = serializedBufferSize;
	size_t existingSize = 0;
	if (LittleFS.exists(LINK_LOG_PATH)) {
		File existingFile = LittleFS.open(LINK_LOG_PATH, "r");
		if (existingFile) {
			existingSize = existingFile.size();
			existingFile.close();
		}
	}
	if (existingSize > LINK_LOG_MAX_SIZE || pendingBytes > LINK_LOG_MAX_SIZE - existingSize) {
		if (LittleFS.exists(LINK_LOG_OLD_PATH) && !LittleFS.remove(LINK_LOG_OLD_PATH)) {
			logWriteError(F("Failed to remove old Wi-Fi link log during rotation"));
			return false;
		}
		if (existingSize > LINK_LOG_MAX_SIZE) {
			// Retire a pre-rotation log that already exceeds the new per-file cap.
			if (!LittleFS.remove(LINK_LOG_PATH)) {
				logWriteError(F("Failed to retire oversized legacy Wi-Fi link log"));
				return false;
			}
			Debug.addLine(F("Retired oversized legacy Wi-Fi link log"));
		} else {
			if (!LittleFS.rename(LINK_LOG_PATH, LINK_LOG_OLD_PATH)) {
				logWriteError(F("Failed to rotate Wi-Fi link log"));
				return false;
			}
			Debug.addLine(F("Rotated Wi-Fi link log to old history file"));
		}
	}

	File file = LittleFS.open(LINK_LOG_PATH, "a");
	if (!file) {
		logWriteError(F("Failed to open Wi-Fi link log in LittleFS"));
		return false;
	}

	bool complete = file.write(reinterpret_cast<const uint8_t *>(linkLogBuffer), pendingBytes) == pendingBytes;
	file.flush();
	file.close();
	if (!complete) {
		logWriteError(F("Failed to write Wi-Fi link log to LittleFS"));
		return false;
	}

	bufferedLines = 0;
	serializedBufferSize = 0;
	bufferSerialized = false;
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
		return;
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
	         "event=%s ok=%lu fail=%lu sta=%lu ap=%lu backoff=%um",
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

	bool foundLog = false;
	const char *logPaths[] = {LINK_LOG_OLD_PATH, LINK_LOG_PATH};
	for (const char *logPath : logPaths) {
		File file = LittleFS.open(logPath, "r");
		if (!file) {
			continue;
		}
		foundLog = true;
		char chunk[128];
		while (file.available()) {
			size_t count = file.readBytes(chunk, sizeof(chunk));
			if (count > 0) {
				httpServer.sendContent(chunk, count);
			}
			yield();
		}
		file.close();
	}
	if (!foundLog) {
		httpServer.sendContent(F("No persisted link events yet.\n"));
	}
	if (bufferSerialized) {
		const char *pending = reinterpret_cast<const char *>(linkLogBuffer);
		for (size_t offset = 0; offset < serializedBufferSize;) {
			size_t count = serializedBufferSize - offset;
			if (count > 128) count = 128;
			httpServer.sendContent(pending + offset, count);
			offset += count;
			yield();
		}
	} else {
		for (uint8_t i = 0; i < bufferedLines; ++i) {
			char formattedLine[LINK_LOG_LINE_SIZE];
			size_t lineLength = formatLinkLogLine(formattedLine, sizeof(formattedLine), linkLogBuffer[i]);
			httpServer.sendContent(formattedLine, lineLength);
		}
	}
	httpServer.sendContent("");
}
