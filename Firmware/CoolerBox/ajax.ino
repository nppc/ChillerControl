void handleAJAXrequest() {
	unsigned long val = millis();
	int tdays = val / 86400000;
	unsigned long sec = val / 1000;
	unsigned long min = sec / 60;
	int hr = min / 60;

	char buf[20];
	sprintf(buf,"%d days, %02d:%02d:%02d",tdays,hr,min % 60,sec % 60);
	String CurTime=String(buf);

	String XML=F("<?xml version='1.0'?>");
	XML += "<response>";
	XML += CurTime;
	XML += "</response>";
	httpServer.send(200,"text/xml",XML);
}