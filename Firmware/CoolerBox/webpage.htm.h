const char HTTP_HEAD_START[] PROGMEM = R"V0G0N(
<!DOCTYPE html>
<html lang="en">
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no" />
<title>Cooler Box Controller</title>
)V0G0N";

const char HTTP_HEAD_END[] PROGMEM = R"V0G0N(
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
<p align="right">Uptime: {curTime}</p>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Temperature (&deg; C)</TD></TR>
<TR><TD><div style="font-size:100px">{curTemp}</div>Set to: {setTemp}</TD></TR>
</TABLE>
<BR>
<TABLE class="values_table">
<TR class="values_header"><TD>Set Voltage</TD><TD>Voltage</TD><TD>Current</TD><TD>PWM</TD></TR>
<TR><TD>{setVoltage}V</TD><TD>{Voltage}V</TD><TD>{Current}A</TD><TD>{PWM}</TD></TR>
</TABLE>
<p>{I2CText}<p>

<TABLE width="100%"><TR>
<TD><button class="btn btn_round" onclick="showhide()">Temperature</button><TD/>
<TD><form action="/settings" method="get"><button class="btn btn_round">Settings</button></form><TD/>
</TABLE>
<div id="settemperature" style="display:none;">
<script>
function NumberChange(n,i) {
    document.getElementById(i).stepUp(n);
    return false;
}

function showhide() {
document.getElementById("settemperature").style.display = (document.getElementById("settemperature").style.display !== "none") ? "none" : "block";
}
</script>
<form action="/setTemp" method="get">
<TABLE>
<tr class="values_header"><td colspan=4>Set Temperature</td></tr>
<tr><td width=80px><button type="button" class="btn btn_minus" onclick="NumberChange(-10,'Temp')">&#171;</button></td>
<td width=80px, style="text-align: center; border-top: 1px solid #0ae;border-bottom: 1px solid #0ae">
<input type="number" name="Temp" id="Temp" class="input" min="0.0" max="20.0" step="0.1" placeholder="(&deg; C)" value="{setTemp}"/></td>
<td width=80px><button type="button" class="btn btn_plus" onclick="NumberChange(10,'Temp')">&#187;</button></td>
<td width=100px><button type="submit" class="btn btn_round">Set</button></td></tr>
</TABLE>
</form>
</div>
)V0G0N";
