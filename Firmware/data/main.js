function NumberChange(n, i) {
  const el = document.getElementById(i);
  if (el) el.stepUp(n);
}

function showhide() {
  const el = document.getElementById("settemperature");
  if (el) el.style.display = (el.style.display !== "none") ? "none" : "block";
}

async function updateStatus() {
  try {
    const res = await fetch('/status');
    if (!res.ok) return;
    const data = await res.json();

    const setIf = (id, val) => {
      const el = document.getElementById(id);
      if (el && val !== undefined) el.textContent = val;
    };

    setIf('Caption', data.Caption);
    setIf('FW', data.FW);
    setIf('CurTime', data.CurTime);
    setIf('I2CStatus', data.I2CStatus);
    setIf('HotTemp', data.HotTemp);
    setIf('boxTemp', data.boxTemp);
    setIf('setHotPWM', data.setHotPWM);
    setIf('Voltage', data.Voltage);
    setIf('Current', data.Current);

    const fanEl = document.getElementById('Fan');
    if (fanEl && data.Fan !== undefined) fanEl.style.backgroundColor = data.Fan;

    const setTempEl = document.getElementById('setTemp');
    if (setTempEl && data.setTemp !== undefined) {
      if (data.stopping) {
        setTempEl.innerHTML = "<div style='color:RED;display:inline-block'>Stopping...</div>";
      } else {
        setTempEl.textContent = data.setTemp;
      }
    }

    const tempInput = document.getElementById('Temp');
    if (tempInput && data.rawSetTemp !== undefined && document.activeElement !== tempInput) {
      tempInput.value = data.rawSetTemp;
    }
  } catch (e) {
    // ignore network errors
  }
}

function setupIndexPage() {
  updateStatus();
  setInterval(updateStatus, 2000);

  const tempForm = document.getElementById('setTempForm');
  if (tempForm) {
    tempForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const tempInput = document.getElementById('Temp');
      if (!tempInput) return;
      try {
        await fetch('/setTemp?Temp=' + encodeURIComponent(tempInput.value));
        showhide();
        await updateStatus();
      } catch (err) {}
    });
  }

  const stopForm = document.getElementById('stopForm');
  if (stopForm) {
    stopForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      if (!confirm('Stop?')) return;
      try {
        await fetch('/stop');
        await updateStatus();
      } catch (err) {}
    });
  }
}

async function setupSettingsPage() {
  try {
    const res = await fetch('/api/settings');
    if (!res.ok) return;
    const data = await res.json();

    const setIf = (id, val) => {
      const el = document.getElementById(id);
      if (el && val !== undefined) el.value = val;
    };

    const capEl = document.getElementById('Caption');
    if (capEl && data.Caption) capEl.textContent = data.Caption;

    setIf('VoltChange', data.changeVoltageSpeed);
    setIf('VoltMin', data.minVoltage);
    setIf('VoltMax', data.maxVoltage);

    const fanDyn = document.getElementById('fanDynamic');
    if (fanDyn) fanDyn.checked = (data.fanDynamic_checked !== 0);

    if (data.boxMode) {
      const radio = document.querySelector(`input[name="boxMode"][value="${data.boxMode}"]`);
      if (radio) radio.checked = true;
    }

    const populateSensors = (selectId, selectedId) => {
      const sel = document.getElementById(selectId);
      if (!sel) return;
      sel.innerHTML = '';
      if (!data.sensors || data.sensors.length === 0) {
        sel.innerHTML = "<option value='' disabled selected>No sensors detected</option>";
        return;
      }
      data.sensors.forEach(s => {
        const opt = document.createElement('option');
        opt.value = s.id;
        if (s.id === selectedId) opt.selected = true;
        opt.textContent = `${s.addr} = ${s.temp.toFixed(2)}° C`;
        sel.appendChild(opt);
      });
    };

    populateSensors('SelColdSensor', data.ColdSensorId);
    populateSensors('SelHotSensor', data.HotSensorId);
  } catch (e) {}
}

async function setupPidsPage() {
  try {
    const res = await fetch('/api/pids');
    if (!res.ok) return;
    const data = await res.json();

    const capEl = document.getElementById('Caption');
    if (capEl && data.Caption) capEl.textContent = data.Caption;

    const setIf = (id, val) => {
      const el = document.getElementById(id);
      if (el && val !== undefined) el.value = val;
    };
    setIf('hotPID_kP', data.hotPID_kP);
    setIf('hotPID_kI', data.hotPID_kI);
    setIf('hotPID_kD', data.hotPID_kD);
  } catch (e) {}
}

async function setupNetworkPage() {
  try {
    const res = await fetch('/api/network');
    if (!res.ok) return;
    const data = await res.json();

    const capEl = document.getElementById('Caption');
    if (capEl && data.Caption) capEl.textContent = data.Caption;

    const setIf = (name, val) => {
      const el = document.querySelector(`input[name="${name}"]`);
      if (el && val !== undefined) el.value = val;
    };
    setIf('apSSID', data.apSSID);
    setIf('apPassword', data.apPassword);
    setIf('IntSSID', data.IntSSID);
    setIf('IntPASS', data.IntPASS);
    setIf('endpointAddress', data.endpointAddress);
    setIf('endpointAccessToken', data.endpointAccessToken);
    setIf('endpointPath', data.endpointPath);
    setIf('sendInterval', data.sendInterval);
  } catch (e) {}
}

async function fetchDebug() {
  try {
    const res = await fetch('/api/debug');
    if (!res.ok) return;
    const data = await res.json();

    const capEl = document.getElementById('Caption');
    if (capEl && data.Caption) capEl.textContent = data.Caption;

    const debugLog = document.getElementById('debugLog');
    if (debugLog && data.debug !== undefined) debugLog.innerHTML = data.debug;
  } catch (e) {}
}

function setupDebugPage() {
  fetchDebug();
  const refreshBtn = document.getElementById('refreshDebugBtn');
  if (refreshBtn) {
    refreshBtn.addEventListener('click', (e) => {
      e.preventDefault();
      fetchDebug();
    });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  if (document.getElementById('boxTemp')) {
    setupIndexPage();
  } else if (document.getElementById('VoltChange')) {
    setupSettingsPage();
  } else if (document.getElementById('hotPID_kP')) {
    setupPidsPage();
  } else if (document.querySelector('input[name="apSSID"]')) {
    setupNetworkPage();
  } else if (document.getElementById('debugLog')) {
    setupDebugPage();
  }
});
