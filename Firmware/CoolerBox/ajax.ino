void handleAJAXrequest() {
	unsigned long val = millis();
	int tdays = val / 86400000;
	unsigned long sec = val / 1000;
	unsigned long min = sec / 60;
	int hr = min / 60;

	char buf[20];
	sprintf(buf,"%d days, %02d:%02d:%02d",tdays,hr,min % 60,sec % 60);
	String CurTime=String(buf);

	String XML=F("<?xml version='1.0'?><xml>");
	XML += "<Tm>" + CurTime + "</Tm>";
	XML += "<HT>" + String(HotTemp,1) + "</HT>";
	XML += "<CT>" + String(ColdTemp,1) + "</CT>";
	XML += "<sV>" + String(setVoltage,1) + "V</sV>";
	XML += "<V>" + String(measuredVoltage,1) + "V</V>";
	XML += "<C>" + String(measuredCurrent,1) + "A</C>";
	XML += "</xml>";
	httpServer.send(200,"text/xml",XML);
}
