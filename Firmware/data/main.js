function NumberChange(n, i) {
  document.getElementById(i).stepUp(n);
}

function showhide() {
  const el = document.getElementById("settemperature");
  el.style.display = (el.style.display !== "none") ? "none" : "block";
}

async function updateData() {
  try {
    const res = await fetch('/status');
    if (!res.ok) return;
    const data = await res.json();

    if (data.I2CStatus !== undefined) document.getElementById('I2CStatus').textContent = data.I2CStatus;
    if (data.CurTime !== undefined) document.getElementById('CurTime').textContent = data.CurTime;
    if (data.HotTemp !== undefined) document.getElementById('HotTemp').textContent = data.HotTemp;
    if (data.boxTemp !== undefined) document.getElementById('boxTemp').textContent = data.boxTemp;
    if (data.setHotPWM !== undefined) document.getElementById('setHotPWM').textContent = data.setHotPWM;
    if (data.setTemp !== undefined) {
      const el = document.getElementById('setTemp');
      if (data.stopping) {
        el.innerHTML = "<div style='color:RED;display:inline-block'>Stopping...</div>";
      } else {
        el.textContent = data.setTemp;
      }
    }
    if (data.Voltage !== undefined) document.getElementById('Voltage').textContent = data.Voltage;
    if (data.Current !== undefined) document.getElementById('Current').textContent = data.Current;
    if (data.Fan !== undefined) document.getElementById('Fan').style.backgroundColor = data.Fan;
  } catch (e) {
    // ignore network errors
  }
}

updateData();
setInterval(updateData, 2000);
