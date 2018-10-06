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
xmlResponse=this.responseXML;
xmldoc = xmlResponse.getElementsByTagName('Tm');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('CurTime').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('HT');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('HotTemp').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('CT');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('ColdTemp').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('sV');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('setVoltage').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('V');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('Voltage').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('C');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('Current').innerHTML=message;

xmldoc = xmlResponse.getElementsByTagName('F');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('Fan').style.backgroundColor = message;

xmldoc = xmlResponse.getElementsByTagName('E');
message = xmldoc[0].firstChild.nodeValue;
document.getElementById('I2CError').innerHTML = message;
}