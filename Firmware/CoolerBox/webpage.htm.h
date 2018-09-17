const char HTTP_HEAD[] PROGMEM = R"V0G0N(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no" />
<title>Cooler Box Controller</title>
<LINK href="webpage.css" rel="stylesheet" type="text/css">
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
<TABLE width=100%><TR><TD><div align="left">Version: {FW}</div></TD><TD><div align="right">Uptime: {curTime}</div></TD></TR></TABLE>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Temperature (&deg;C)</TD></TR>
<TR><TD><div style="font-size:100px">{ColdTemp}</div>Set to: {setTemp}</TD></TR>
</TABLE>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Set Voltage</TD><TD>Voltage</TD><TD>Current</TD><TD>Hot side</TD></TR>
<TR><TD>{setVoltage}V</TD><TD>{Voltage}V</TD><TD>{Current}A</TD><TD>{HotTemp}&deg; C</TD></TR>
</TABLE>
<p>{I2CText}<p>

<TABLE width="100%"><TR>
<TD><button class="btn btn_round" onclick="showhide()">Temperature</button><TD/>
<TD><form action="/settings" method="get"><button class="btn btn_round">Settings</button></form><TD/>
<TD><form action="/stop" method="get"><button class="btn btn_round btn_red">STOP</button></form><TD/></TR>
</TABLE>
<div id="settemperature" style="display:none;">
<script>
function NumberChange(n,i){document.getElementById(i).stepUp(n);}
function showhide() {
document.getElementById("settemperature").style.display = (document.getElementById("settemperature").style.display !== "none") ? "none" : "block";
}
</script>
<form action="/setTemp" method="get">
<TABLE>
<tr class="values_header"><td colspan=4>Set Temperature</td></tr>
<tr><td width=80px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'Temp')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="Temp" id="Temp" class="input" min="0.0" max="25.0" step="0.1" placeholder="(&deg;C)" value="{setTemp}"/></td>
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
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<TABLE class="values_table">
<BR>
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
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save Settings</button></form></TD>
<TD><form action="/update" method="get"><button type="submit" class="btn btn_round">Upload Firmware</button></form></TD>
<TD><form action="/pids" method="get"><button type="submit" class="btn btn_round">PIDs</button></form></TD></TR>
<TR><TD><form action="/network" method="get"><button type="submit" class="btn btn_round">Network</button></form></TD>
<TD><form action="/filelist" method="get"><button type="submit" class="btn btn_round">File Browser</button></form></TD></TR>
</TABLE>
)V0G0N";

const char HTTP_PIDS_DATA[] PROGMEM = R"V0G0N(
<BR>
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<form action="/pids_store" method="get">
<TABLE class="values_table">
<BR>
<tr class="values_header"><td colspan=4>P</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'pid_kP')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="pid_kP" id="pid_kP" class="input" min="0.0" max="300.0" step="0.1" value="{pid_kP}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'pid_kP')">&#187;</button></td>
<TD rowspan=2>Current value is {pid_kP}</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>I</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'pid_kI')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="pid_kI" id="pid_kI" class="input" min="0.0" max="100.0" step="0.1" value="{pid_kI}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'pid_kI')">&#187;</button></td>
<TD rowspan=2>Current value is {pid_kI}</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>D</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'pid_kD')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="pid_kD" id="pid_kD" class="input" min="0.0" max="300.0" step="0.1" value="{pid_kD}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'pid_kD')">&#187;</button></td>
<TD rowspan=2>Current value is {pid_kD}</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
</TABLE>
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save PIDs</button></TD></TR>
</TABLE>
</form>
)V0G0N";

const char HTTP_NETWORK_DATA[] PROGMEM = R"V0G0N(
<BR>
<script>function NumberChange(n,i){document.getElementById(i).stepUp(n);}</script>
<form action="/network_store" method="get">
<TABLE class="values_table">
<BR>
<tr class="values_header"><td colspan=4>SSID</td></tr>
<tr><td colspan=3><input type="text" name="IntSSID" class="inputtext" value="{IntSSID}"/></td>
<TD rowspan=2>SSID of the Access Point that has connection to the Internet</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Password</td></tr>
<tr><td colspan=3><input type="text" name="IntPASS" class="inputtext" value="{IntPASS}"/></td>
<TD rowspan=2>Password for SSID</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>ThingSpeak Write API Key</td></tr>
<tr><td colspan=3><input type="text" name="thingWriteAPIKey" class="inputtext" value="{thingWriteAPIKey}"/></td>
<TD rowspan=2>Use this key to write data to a channel.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Ubidots Token</td></tr>
<tr><td colspan=3><input type="text" name="ubiToken" class="inputtext" value="{ubiToken}"/></td>
<TD rowspan=2>Use this Authentications Token to access Ubidots.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Ubidots Device API Label</td></tr>
<tr><td colspan=3><input type="text" name="ubiDevice" class="inputtext" value="{ubiDevice}"/></td>
<TD rowspan=2>Use this Label to indicate the Device you accessing.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>Interval (s)</td></tr>
<tr><td width=80px height=60px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'sendInterval')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="sendInterval" id="sendInterval" class="input" min="10" max="86400" step="1" value="{sendInterval}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'sendInterval')">&#187;</button></td>
<TD rowspan=2>Interval of sending data in seconds.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
<tr class="values_header"><td colspan=4>IoT Services</td></tr>
<tr><td colspan=3 style="line-height: 2;text-align: left">
<input type="checkbox" name="sendThing" id="sendThing" {sendThing_checked}><label for="sendThing"> ThingSpeak</label>
<BR>
<input type="checkbox" name="sendUbi" id="sendUbi" {sendUbi_checked}><label for="sendUbi"> Ubidots</label>
</td>
<TD rowspan=2>Select to what services values will be sent.</TD></tr>
<TR><TD colspan=3>&nbsp;</TD><TR>
</TABLE>
<TABLE width="100%">
<TR><TD><button type="submit" class="btn btn_round">Save Network Settings</button></TD></TR>
</TABLE>
</form>
)V0G0N";
