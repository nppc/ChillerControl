const char HTTP_HEADweb[] PROGMEM = R"V0G0N(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no" />
<title>Cooler Box Controller</title>
<LINK href="/webpage.css" rel="stylesheet" type="text/css">
</head>
<body>
<div class="container">
<table class="hdr_table">
<tr><td class="hdr_caption">{Caption}</td>
<td class="hdr_logo">
<div class="hdr_logo_text_short">COOLER</div>
<div class="hdr_logo_text_long">Peltie Voltage Controller</div>
</td></tr></table>
)V0G0N";

const char HTTP_END[] PROGMEM = R"V0G0N(
</div>
</body>
</html>
)V0G0N";

const char HTTP_MAIN_DATA[] PROGMEM = R"V0G0N(
<script src="/main.js"></script>
<TABLE width=100%><TR><TD><div align="left">Version: {FW}</div></TD><TD><div align="right">Uptime: <div id="CurTime" style="display:inline">loading...</div></div></TD></TR></TABLE>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Temperature (&deg;C)</TD></TR>
<TR><TD><div style="font-size:100px" id="boxTemp">{boxTemp}</div>Set to: <div id="setTemp" style="display:inline">{setTemp}</div></TD></TR>
</TABLE>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Hot PWM</TD><TD>Cold Volt</TD><TD>Cold Curr</TD><TD>Hot side</TD></TR>
<TR><TD id="setHotPWM">{setHotPWM}</TD><TD id="Voltage">{Voltage}</TD><TD id="Current">{Current}</TD><TD id="Fan"><div style="display:inline" id="HotTemp">{HotTemp}</div>&deg;&nbsp;C</TD></TR>
</TABLE>
<div style="line-height:3" id="I2CStatus">{I2CText}</div>
<TABLE width="100%"><TR>
<TD><button class="btn btn_round" onclick="showhide()">Temperature</button><TD/>
<TD><form action="/settings" method="get"><button class="btn btn_round">Settings</button></form><TD/>
<TD><form action="/stop" method="get"><button class="btn btn_round btn_red" onclick="return confirm('Stop?')">STOP</button></form><TD/></TR>
</TABLE>
<div id="settemperature" style="display:none;">
<form action="/setTemp" method="get">
<TABLE>
<tr class="values_header"><td colspan=4>Set Temperature</td></tr>
<tr><td width=80px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'Temp')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="Temp" id="Temp" class="input" min="0.0" max="45.0" step="0.1" placeholder="(&deg;C)" value="{setTemp}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'Temp')">&#187;</button></td>
<td width=100px><button type="submit" class="btn btn_round">Set</button></td></tr>
</TABLE>
</form>
</div>
)V0G0N";

const char HTTP_SETTINGS_DATA[] PROGMEM = R"V0G0N(
<BR>
<form action="/settings_store" method="get">
<TABLE class="values_table">
<TR class="values_header"><TD colspan=2>Temperature Sensors</TD></TR>
<TR><TD>Cold Sensor </TD><TD><select name="SelColdSensor">{ColdDSlist}</select></TD></TR>
<TR><TD>Hot Sensor </TD><TD><select name="SelHotSensor">{HotDSlist}</select></TD></TR>
</TABLE>
<BR>
<TABLE class="values_table">
<tr class="values_header"><td colspan=2>Cooling/Heating</td></tr>
<tr><TD width=240px, style="line-height: 2;text-align: left">
    <input name="boxMode" type="radio" value="1" {boxMode_1}> Cooling/Heating<BR>
    <input name="boxMode" type="radio" value="2" {boxMode_2}> Cooling<BR>
    <input name="boxMode" type="radio" value="3" {boxMode_3}> Heating<BR>
</TD>
<TD rowspan=2>To adjust temperature inside, device can cool or heat air, just cool, or just heat. </TD></tr>
<TR><TD>&nbsp;</TD><TR>
</TABLE>
<BR>
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<TABLE class="values_table">
<tr class="values_header"><td colspan=4>Voltage change (V/S)</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'VoltChange')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="VoltChange" id="VoltChange" class="input" min="0.4" max="25.0" step="0.1" placeholder="V" value="{changeVoltageSpeed}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'VoltChange')">&#187;</button></td>
<TD rowspan=2>How fast voltage allowed to change inside DC/DC Buck converter. For example, with value 1.0 voltage will rise from 0V to 5V within 5 seconds.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Minimum Voltage (V)</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'VoltMin')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="VoltMin" id="VoltMin" class="input" min="1.0" max="12.0" step="0.1" placeholder="V" value="{minVoltage}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'VoltMin')">&#187;</button></td>
<TD rowspan=2>Minimum voltage that is allowed for the DC/DC to be during temperature regulation. 0 Also allowed, meaning DC/DC is off.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Maximum Voltage (V)</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'VoltMax')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="VoltMax" id="VoltMax" class="input" min="1.0" max="12.0" step="0.1" placeholder="V" value="{maxVoltage}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'VoltMax')">&#187;</button></td>
<TD rowspan=2>Maximum voltage that is allowed for the DC/DC to be during temperature regulation.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
</TABLE>
<BR>
<TABLE class="values_table">
<tr class="values_header"><td colspan=2>Dynamic Fan Control</td></tr>
<tr><TD width=240px, style="line-height: 2;text-align: left">
<input type="checkbox" name="fanDynamic" id="fanDynamic" {fanDynamic_checked}><label for="fanDynamic"> Enable Dynamic control</label>
</TD>
<TD rowspan=2>Cooling fan on the top of the fridge can be turned off automatically if radiator is not hot.</TD></tr>
<TR><TD>&nbsp;</TD><TR>
</TABLE>
<BR>
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save Settings</button></form></TD>
<TD><form action="/update" method="get"><button type="submit" class="btn btn_round">Upload Firmware</button></form></TD>
<TD><form action="/pids" method="get"><button type="submit" class="btn btn_round">PIDs</button></form></TD></TR>
<TR><TD><form action="/network" method="get"><button type="submit" class="btn btn_round">Network</button></form></TD>
<TD><form action="/filelist" method="get"><button type="submit" class="btn btn_round">File Browser</button></form></TD>
<TD><form action="/debug" method="get"><button type="submit" class="btn btn_round">Debug</button></form></TD></TR>
</TABLE>
)V0G0N";

const char HTTP_PIDS_DATA[] PROGMEM = R"V0G0N(
<BR>
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<form action="/pids_store" method="get">
<TABLE class="values_table">
<BR>
<tr class="values_header"><td colspan=4>Hot PIDs</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'hotPID_kP')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="hotPID_kP" id="hotPID_kP" class="input" min="0.0" max="300.0" step="0.1" value="{hotPID_kP}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'hotPID_kP')">&#187;</button></td>
<TD>P</TD></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-1,'hotPID_kI')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="hotPID_kI" id="hotPID_kI" class="input" min="0.0" max="100.0" step="0.001" value="{hotPID_kI}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(1,'hotPID_kI')">&#187;</button></td>
<TD>I</TD></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'hotPID_kD')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="hotPID_kD" id="hotPID_kD" class="input" min="0.0" max="300.0" step="0.1" value="{hotPID_kD}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'hotPID_kD')">&#187;</button></td>
<TD>D</TD></tr>
</TABLE>
<BR>
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save PIDs</button></TD></TR>
</TABLE>
</form>
)V0G0N";

const char HTTP_NETWORK_DATA[] PROGMEM = R"V0G0N(
<BR>
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<form action="/network_store" method="post">
<TABLE class="values_table">
<BR>
<tr class="values_header"><td colspan=4>Access Point SSID</td></tr>
<tr><td colspan=3><input type="text" name="apSSID" class="inputtext" maxlength="32" value="{apSSID}"/></td>
<TD rowspan=2>SSID of the device Access Point</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Access Point Password</td></tr>
<tr><td colspan=3><input type="text" name="apPassword" class="inputtext" maxlength="63" value="{apPassword}"/></td>
<TD rowspan=2>Password for device Access Point (at least 8 chars, or empty for open)</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Internet WiFi SSID</td></tr>
<tr><td colspan=3><input type="text" name="IntSSID" class="inputtext" value="{IntSSID}"/></td>
<TD rowspan=2>SSID of the Access Point that has connection to the Internet</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Internet WiFi Password</td></tr>
<tr><td colspan=3><input type="text" name="IntPASS" class="inputtext" value="{IntPASS}"/></td>
<TD rowspan=2>Password for SSID</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Endpoint Address</td></tr>
<tr><td colspan=3><input type="text" name="endpointAddress" class="inputtext" value="{endpointAddress}"/></td>
<TD rowspan=2>HTTPS URL prefix, for example https://aa.ddnss.eu/api/v1</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Access Token</td></tr>
<tr><td colspan=3><input type="password" name="endpointAccessToken" class="inputtext" value="{endpointAccessToken}"/></td>
<TD rowspan=2>Inserted as a path segment between the address and path.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Endpoint Path</td></tr>
<tr><td colspan=3><input type="text" name="endpointPath" class="inputtext" value="{endpointPath}"/></td>
<TD rowspan=2>Path suffix, for example /telemetry</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Interval (s)</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'sendInterval')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="sendInterval" id="sendInterval" class="input" min="10" max="86400" step="1" value="{sendInterval}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'sendInterval')">&#187;</button></td>
<TD rowspan=2>Interval of sending data in seconds.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
</TABLE>
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save Network Settings & Reboot</button></TD></TR>
</TABLE>
</form>
)V0G0N";

const char HTTP_DEBUG_DATA[] PROGMEM = R"V0G0N(
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Debug Data</TD></TR>
<TR><TD><pre style="font-family:monospace;text-align:left;">{Debug}</pre></TD></TR>
</TABLE>
<BR>
<TABLE width="100%">
<TR><TD><form action="/debug" method="get"><button type="submit" class="btn btn_round">Refresh</button></form></TD></TR>
</TABLE>
)V0G0N";

const char HTTP_CSS[] PROGMEM = R"V0G0N(
<style>
table {width: 100%;}
.hdr_table {width: 100%; padding-top: 7px}
.hdr_caption {padding: 5px;text-align: left;vertical-align: bottom; font-family: 'Verdana'; font-size: 32px;border-right: 2px solid LIGHTGRAY;border-bottom: 2px solid LIGHTGRAY;font-weight: bold}
.hdr_logo {width: 120px;text-align: right;border-left: 2px solid LIGHTGRAY;border-top: 2px solid LIGHTGRAY;font-weight: bold}
.hdr_logo_text_short {font-family: 'Verdana'; font-size: 28px; color: LIGHTGRAY;margin-top: -6px;}
.hdr_logo_text_long {font-family: 'Verdana'; font-size: 10px; margin-top: -5px; margin-right: 2px;color: GRAY;font-weight: normal}

.values_table {width: 100%;border: 1px solid #DDDDDD;border-spacing:0px; border-top: 0px;margin: 0 auto;}
.values_header {text-align: left;font-weight: bold; color: GRAY;background: #DDDDDD;;padding: 5px}
.values_table th{text-align: center; background: #EEEEEE;border-left: 1px solid #DDDDDD; border-right: 1px solid #DDDDDD;padding: 5px;font-weight: normal;}
.values_table td{text-align: center; border-left: 1px solid #DDDDDD; border-right: 1px solid #DDDDDD; border-bottom: 1px solid #DDDDDD;padding: 5px}
.values_table_bottom{padding: 5px; padding-left: 100px; padding-right: 100px}

body,textarea,input,select{background: 0;border-radius: 0;margin: 0;font-family: 'Verdana';font-size: 16px}
.container{margin: auto;width: 90%}@media(min-width:1200px){.container{margin: auto;width: 30%}}@media(min-width:768px) and (max-width:1200px){.container{margin: auto;width: 50%}}
.btn_minus{font-size:40px;border-top-left-radius:20px;border-bottom-left-radius:20px}
.btn_plus{font-size:40px;border-top-right-radius:20px;border-bottom-right-radius:20px}
.btn_round{font-size:20px;border-radius:20px}
.btn_small{font-size:20px;border-radius:8px}
.btn{background: #0ae;border: 0;color: #fff;cursor: pointer;display: inline-block;height: 60px;width: 100%}
.btn:hover{background: #09d}
.btn:active,.btn:focus{background: #08b}
.btn_s{background: #0ae;border: 0;color: #fff;cursor: pointer;display: inline-block;height: 30px;width: 30px}
.btn_red{background: #ff0000;border: 0;color: #fff;cursor: pointer;display: inline-block}
.btn_red:hover{background: #f00000}

.input{width: 60px;text-align: right;}
.inputtext{width: 200px;text-align: left;}
.tblhto {border-collapse: collapse;}
.tblhto th, .tblhto td {text-align: left;border: 1px solid;}
</style>
)V0G0N";

const char MAIN_JS[] PROGMEM = R"V0G0N(
var x = setInterval(function() {loadData("ajax",updateData)}, 5000);
function NumberChange(n,i){document.getElementById(i).stepUp(n);}
function showhide() {
document.getElementById("settemperature").style.display = (document.getElementById("settemperature").style.display !== "none") ? "none" : "block";
}
function loadData(url, callback){
var xhttp = new XMLHttpRequest();
xhttp.onreadystatechange = function(){
 if(this.readyState == 4 && this.status == 200){
 callback.apply(xhttp);
 }
};
xhttp.open("GET", url, true);
xhttp.send();
}
function updateData(){
var xmlResponse = this.responseXML;
if(!xmlResponse) return;
function getNodeVal(tag){
var el = xmlResponse.getElementsByTagName(tag);
return (el && el.length > 0 && el[0].firstChild) ? el[0].firstChild.nodeValue : null;
}
var val;
val = getNodeVal('I');
if(val !== null) document.getElementById('I2CStatus').innerHTML = val;
val = getNodeVal('Tm');
if(val !== null) document.getElementById('CurTime').innerHTML = val;
val = getNodeVal('HT');
if(val !== null) document.getElementById('HotTemp').innerHTML = val;
val = getNodeVal('CT');
if(val !== null) document.getElementById('boxTemp').innerHTML = val;
val = getNodeVal('sV');
if(val !== null) document.getElementById('setHotPWM').innerHTML = val;
val = getNodeVal('sT');
if(val !== null){
if(val === "Stopping..."){
document.getElementById('setTemp').innerHTML = "<div style='color:RED;display:inline-block'>Stopping...</div>";
}else{
document.getElementById('setTemp').innerHTML = val;
}
}
val = getNodeVal('V');
if(val !== null) document.getElementById('Voltage').innerHTML = val;
val = getNodeVal('C');
if(val !== null) document.getElementById('Current').innerHTML = val;
val = getNodeVal('F');
if(val !== null) document.getElementById('Fan').style.backgroundColor = val;
}
)V0G0N";
