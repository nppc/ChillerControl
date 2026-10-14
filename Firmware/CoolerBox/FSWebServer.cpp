#include "CoolerBox.h"

String getContentType(const String &filename) {
	if (filename.endsWith(F(".htm")) || filename.endsWith(F(".html"))) return F("text/html; charset=utf-8");
	if (filename.endsWith(F(".css"))) return F("text/css; charset=utf-8");
	if (filename.endsWith(F(".js"))) return F("application/javascript; charset=utf-8");
	if (filename.endsWith(F(".json"))) return F("application/json; charset=utf-8");
	if (filename.endsWith(F(".ico"))) return F("image/x-icon");
	if (filename.endsWith(F(".gz"))) return F("application/x-gzip");
	return F("text/plain; charset=utf-8");
}

bool handleFileRead(String path) {
	if (!path.startsWith("/")) path = "/" + path;
	String contentType = getContentType(path);
	String pathWithGz = path + ".gz";
	if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) {
		if (LittleFS.exists(pathWithGz)) {
			path += ".gz";
		}
		if (path.indexOf(".css") >= 0 || path.indexOf(".js") >= 0 || path.indexOf(".ico") >= 0) {
			httpServer.sendHeader("Cache-Control", "public, max-age=86400");
		} else if (path.indexOf(".htm") >= 0 || path.indexOf(".html") >= 0) {
			httpServer.sendHeader("Cache-Control", "no-cache");
		}
		File file = LittleFS.open(path, "r");
		httpServer.streamFile(file, contentType);
		file.close();
		return true;
	}
	return false;
}

void handleFSWebServer() {
	httpServer.onNotFound([]() {
		if (!handleFileRead(httpServer.uri())) {
			httpServer.send(404, "text/plain", "FileNotFound");
		}
	});
}