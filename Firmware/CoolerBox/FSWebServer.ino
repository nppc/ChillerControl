File fsUploadFile;

//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

String getContentType(String filename) {
  if (httpServer.hasArg("download")) {
    return "application/octet-stream";
  } else if (filename.endsWith(".htm")) {
    return "text/html";
  } else if (filename.endsWith(".html")) {
    return "text/html";
  } else if (filename.endsWith(".css")) {
    return "text/css";
  } else if (filename.endsWith(".js")) {
    return "application/javascript";
  } else if (filename.endsWith(".png")) {
    return "image/png";
  } else if (filename.endsWith(".gif")) {
    return "image/gif";
  } else if (filename.endsWith(".jpg")) {
    return "image/jpeg";
  } else if (filename.endsWith(".ico")) {
    return "image/x-icon";
  } else if (filename.endsWith(".xml")) {
    return "text/xml";
  } else if (filename.endsWith(".pdf")) {
    return "application/x-pdf";
  } else if (filename.endsWith(".zip")) {
    return "application/x-zip";
  } else if (filename.endsWith(".gz")) {
    return "application/x-gzip";
  }
  return "text/plain";
}

bool handleFileRead(String path) {
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {
    if (SPIFFS.exists(pathWithGz)) {
      path += ".gz";
    }
    File file = SPIFFS.open(path, "r");
    httpServer.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void handleFileUpload() {
  HTTPUpload& upload = httpServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {filename = "/" + filename;}
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {
      fsUploadFile.close();
    }
  }
  	httpServer.sendHeader("Location", String("/filelist"), true);
	httpServer.send ( 302, "text/plain", "");

}

void handleFileDelete() {
	if (httpServer.args() == 0) {
	return httpServer.send(500, "text/plain", "BAD ARGS");
	}
	String path = httpServer.arg(0);
	if (path == "/") {
	return httpServer.send(500, "text/plain", "BAD PATH");
	}
	if (!SPIFFS.exists(path)) {
	return httpServer.send(404, "text/plain", "FileNotFound");
	}
	SPIFFS.remove(path);
	httpServer.sendHeader("Location", String("/filelist"), true);
	httpServer.send ( 302, "text/plain", "");
}


void handleFileList() {

//  String path = httpServer.arg("dir");
  Dir dir = SPIFFS.openDir("/");
//  path = String();

  String output = "<HTML><BODY><TABLE>";
  while (dir.next()) {
    File entry = dir.openFile("r");
    output += "<TR><TD>";
    bool isDir = false;
    output += (isDir) ? "D" : "F";
    output += "</TD><TD>";
    output += (isDir) ? "" : "<a href=/file_download?filename="+String(entry.name()).substring(1)+">";
    output += String(entry.name()).substring(1);
    output += (isDir) ? "" : "</a>";
    output += "</TD><TD>[<a href=/file_delete?filename=/";
    output += String(entry.name()).substring(1);
	output += ">X</a>]</TD></TR>";
    entry.close();
  }

  output += "</TABLE><BR>";
  output += "<form method='POST' action='/filelist' enctype='multipart/form-data'>";
  output += "<input type='file' name='filename'><input type='submit'></form>";
  output += "</BODY></HTML>";
  httpServer.send(200, "text/html", output);
}

void file_download(){
	if (httpServer.args() == 1 ) {
		String filename= httpServer.arg(0);

		File download = SPIFFS.open("/"+filename, "r");
		if (download) {
		  httpServer.sendHeader("Content-Type", "text/text");
		  httpServer.sendHeader("Content-Disposition", "attachment; filename="+filename);
		  httpServer.sendHeader("Connection", "close");
		  httpServer.streamFile(download, "application/octet-stream");
		  download.close();
		} else {} 
	}
}

void handleFSWebServer(){
  //list directory
  httpServer.on("/filelist", HTTP_GET, handleFileList);

  httpServer.on("/file_download", HTTP_GET, file_download);
  
  //create file
  //httpServer.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  httpServer.on("/file_delete", HTTP_GET, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  httpServer.on("/filelist", HTTP_POST, []() {
    httpServer.send(200, "text/plain", "");
  }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  httpServer.onNotFound([]() {
    if (!handleFileRead(httpServer.uri())) {
      httpServer.send(404, "text/plain", "FileNotFound");
    }
  });
}