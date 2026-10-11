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

