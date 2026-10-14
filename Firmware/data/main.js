function NumberChange(n, i) {
  const el = document.getElementById(i);
  if (!el) return;
  if (!el.value || el.value === '') {
    const setTempEl = document.getElementById('setTemp');
    const current = setTempEl ? parseFloat(setTempEl.textContent) : 19.0;
    el.value = (isNaN(current) ? 19.0 : current).toFixed(1);
  }
  el.stepUp(n);
}

function setupNavigation() {
  const container = document.querySelector('.container');
  if (!container) return;

  const oldHeader = container.querySelector('.hdr_table');
  const caption = oldHeader && oldHeader.querySelector('.hdr_caption');
  const header = document.createElement('header');
  header.className = 'app-header';

  const menuButton = document.createElement('button');
  menuButton.type = 'button';
  menuButton.className = 'menu-toggle';
  menuButton.id = 'menuBtn';
  menuButton.setAttribute('aria-label', 'Open navigation menu');
  menuButton.setAttribute('aria-controls', 'sideMenu');
  menuButton.setAttribute('aria-expanded', 'false');
  menuButton.textContent = '\u2630';

  const brandCopy = document.createElement('div');
  brandCopy.className = 'brand-copy';
  const brandName = document.createElement('div');
  brandName.className = 'brand-name';
  brandName.textContent = 'COOLER BOX';
  const title = document.createElement('h1');
  title.id = 'appTitle';
  title.textContent = caption ? caption.textContent.trim() : document.title;
  brandCopy.append(brandName, title);
  header.append(menuButton, brandCopy);
  container.insertBefore(header, container.firstChild);

  const menu = document.createElement('nav');
  menu.id = 'sideMenu';
  menu.setAttribute('aria-label', 'Main navigation');
  const menuHeading = document.createElement('h2');
  menuHeading.textContent = 'Menu';
  menu.appendChild(menuHeading);

  const menuItems = [
    ['Home', '/'],
    ['Settings', '/settings'],
    ['PID tuning', '/pids'],
    ['Network', '/network'],
    ['Diagnostics', '/debug'],
    ['Firmware update', '/update']
  ];
  menuItems.forEach(([label, href]) => {
    const link = document.createElement('a');
    link.className = 'menu-item';
    link.href = href;
    link.textContent = label;
    if (window.location.pathname === href ||
        (href === '/' && window.location.pathname === '/index.htm')) {
      link.setAttribute('aria-current', 'page');
    }
    menu.appendChild(link);
  });

  const backdrop = document.createElement('button');
  backdrop.type = 'button';
  backdrop.id = 'sideMenuBackdrop';
  backdrop.setAttribute('aria-label', 'Close navigation menu');
  document.body.append(menu, backdrop);

  const closeMenu = () => {
    menu.classList.remove('open');
    backdrop.classList.remove('open');
    menuButton.setAttribute('aria-expanded', 'false');
    menuButton.setAttribute('aria-label', 'Open navigation menu');
  };
  menuButton.addEventListener('click', () => {
    const isOpen = menu.classList.toggle('open');
    backdrop.classList.toggle('open', isOpen);
    menuButton.setAttribute('aria-expanded', String(isOpen));
    menuButton.setAttribute('aria-label', isOpen ? 'Close navigation menu' : 'Open navigation menu');
    if (isOpen) menu.querySelector('a').focus();
  });
  backdrop.addEventListener('click', closeMenu);
  menu.addEventListener('click', (event) => {
    if (event.target.closest('a')) closeMenu();
  });
  document.addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && menu.classList.contains('open')) {
      closeMenu();
      menuButton.focus();
    }
  });

  if (caption) {
    new MutationObserver(() => {
      title.textContent = caption.textContent.trim();
    }).observe(caption, { childList: true, characterData: true, subtree: true });
  }
}

function showhide() {
  const el = document.getElementById("settemperature");
  if (!el) return;
  const isHidden = (el.style.display === "none" || !el.style.display);
  el.style.display = isHidden ? "block" : "none";
  if (isHidden) {
    const tempInput = document.getElementById("Temp");
    const setTempEl = document.getElementById("setTemp");
    if (tempInput && setTempEl) {
      const current = parseFloat(setTempEl.textContent);
      if (!isNaN(current)) tempInput.value = current.toFixed(1);
      tempInput.focus();
    }
  }
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
      setTempEl.textContent = data.setTemp;
    }

    const stopBadge = document.getElementById('stopBadge');
    if (stopBadge) {
      stopBadge.style.display = data.stopping ? 'inline' : 'none';
    }

    const controlBtn = document.getElementById('controlBtn');
    const controlForm = document.getElementById('controlForm');
    if (controlBtn && controlForm) {
      if (data.stopping) {
        controlBtn.textContent = 'START';
        controlBtn.className = 'btn btn_round btn_green';
        controlForm.action = '/start';
      } else {
        controlBtn.textContent = 'STOP';
        controlBtn.className = 'btn btn_round btn_red';
        controlForm.action = '/stop';
      }
    }

    const setTempPanel = document.getElementById('settemperature');
    const isEditing = setTempPanel && setTempPanel.style.display !== 'none';
    const tempInput = document.getElementById('Temp');
    if (tempInput && data.rawSetTemp !== undefined && !isEditing && document.activeElement !== tempInput) {
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
      if (!tempInput || !tempInput.value) return;
      const val = parseFloat(tempInput.value);
      if (isNaN(val) || val < 0 || val > 45) {
        alert('Please enter a temperature between 0.0 and 45.0 \u00B0C');
        return;
      }
      const setBtn = tempForm.querySelector('button[type="submit"]');
      if (setBtn) setBtn.disabled = true;

      const setTempEl = document.getElementById('setTemp');
      if (setTempEl) setTempEl.textContent = val.toFixed(1);

      try {
        const res = await fetch('/setTemp?Temp=' + encodeURIComponent(val.toFixed(1)));
        if (res.ok) {
          const panel = document.getElementById('settemperature');
          if (panel) panel.style.display = 'none';
        }
      } catch (err) {
      } finally {
        if (setBtn) setBtn.disabled = false;
        await updateStatus();
      }
    });
  }

  const controlForm = document.getElementById('controlForm');
  if (controlForm) {
    controlForm.addEventListener('submit', async (e) => {
      e.preventDefault();
      const isStopping = (controlForm.action.indexOf('/stop') >= 0);
      if (isStopping && !confirm('Stop?')) return;
      try {
        await fetch(isStopping ? '/stop' : '/start');
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
        opt.textContent = `${s.addr} = ${s.temp.toFixed(2)}\u00B0 C`;
        sel.appendChild(opt);
      });
    };

    populateSensors('SelColdSensor', data.ColdSensorId);
    populateSensors('SelHotSensor', data.HotSensorId);
  } catch (e) {}
}

let autotuneTimer = null;
let isAutotuning = false;

function initAutotuneTarget(targetVal) {
  const targetInput = document.getElementById('atTargetTemp');
  if (!targetInput) return;
  if (!targetInput.dataset.userEdited) {
    const saved = localStorage.getItem('autotune_target');
    const val = saved ? parseFloat(saved) : (targetVal || 19.0);
    if (!isNaN(val)) {
      targetInput.value = val.toFixed(1);
    }
  }
  if (!targetInput.dataset.listenerAttached) {
    targetInput.dataset.listenerAttached = 'true';
    targetInput.addEventListener('input', () => {
      targetInput.dataset.userEdited = 'true';
      const v = parseFloat(targetInput.value);
      if (!isNaN(v) && v >= 5 && v <= 45) {
        localStorage.setItem('autotune_target', v.toFixed(1));
      }
    });
  }
}

function initAutotunePwm(pwmVal) {
  const pwmInput = document.getElementById('atTestPWM');
  if (!pwmInput) return;
  if (!pwmInput.dataset.userEdited) {
    const saved = localStorage.getItem('autotune_pwm');
    const val = saved ? parseInt(saved, 10) : (pwmVal || 300);
    if (!isNaN(val)) {
      pwmInput.value = val;
    }
  }
  if (!pwmInput.dataset.listenerAttached) {
    pwmInput.dataset.listenerAttached = 'true';
    pwmInput.addEventListener('input', () => {
      pwmInput.dataset.userEdited = 'true';
      const v = parseInt(pwmInput.value, 10);
      if (!isNaN(v) && v >= 100 && v <= 700) {
        localStorage.setItem('autotune_pwm', v.toString());
      }
    });
  }
}

async function pollAutotuneStatus() {
  try {
    const res = await fetch('/api/autotune');
    if (!res.ok) return;
    const data = await res.json();

    const btn = document.getElementById('btnAutotune');
    const statusBox = document.getElementById('atStatusBox');
    const detailsBox = document.getElementById('atDetailsBox');
    const targetInput = document.getElementById('atTargetTemp');
    const pwmInput = document.getElementById('atTestPWM');

    isAutotuning = data.active;
    if (btn) {
      if (data.active) {
        btn.textContent = 'Stop Autotune';
        btn.className = 'btn btn_round btn_red';
      } else {
        btn.textContent = 'Start Autotune';
        btn.className = 'btn btn_round btn_green';
      }
    }

    if (targetInput) {
      if (data.active && data.targetTemp !== undefined) {
        targetInput.value = parseFloat(data.targetTemp).toFixed(1);
      } else {
        initAutotuneTarget(data.targetTemp || data.setTemp);
      }
    }

    if (pwmInput) {
      if (data.active && data.testPWM !== undefined) {
        pwmInput.value = parseInt(data.testPWM, 10);
      } else {
        initAutotunePwm(data.testPWM || 300);
      }
    }

    if (statusBox) {
      statusBox.textContent = `Status: ${data.message || data.stateStr}`;
    }

    if (detailsBox) {
      if (data.active) {
        detailsBox.textContent = `Cycle ${data.cycle}/${data.targetCycles} | Temp: ${data.currentTemp.toFixed(2)}\u00B0C (Pk: ${data.peakTemp.toFixed(2)}\u00B0C, Vl: ${data.valleyTemp.toFixed(2)}\u00B0C)`;
      } else if (data.calculatedKp > 0) {
        detailsBox.textContent = `Calculated: Kp=${data.calculatedKp.toFixed(2)}, Ki=${data.calculatedKi.toFixed(4)}, Kd=${data.calculatedKd.toFixed(2)}`;
        const setIf = (id, val) => {
          const el = document.getElementById(id);
          if (el && val !== undefined) el.value = val;
        };
        setIf('hotPID_kP', data.calculatedKp.toFixed(1));
        setIf('hotPID_kI', data.calculatedKi.toFixed(3));
        setIf('hotPID_kD', data.calculatedKd.toFixed(1));
      } else {
        detailsBox.textContent = '';
      }
    }
  } catch (e) {}
}

async function toggleAutotune() {
  const targetTempInput = document.getElementById('atTargetTemp');
  const targetTemp = targetTempInput && targetTempInput.value ? parseFloat(targetTempInput.value) : 20.0;
  if (!isNaN(targetTemp)) {
    localStorage.setItem('autotune_target', targetTemp.toFixed(1));
  }

  const pwmInput = document.getElementById('atTestPWM');
  const testPwm = pwmInput && pwmInput.value ? parseInt(pwmInput.value, 10) : 300;
  if (!isNaN(testPwm)) {
    localStorage.setItem('autotune_pwm', testPwm.toString());
  }

  const action = isAutotuning ? 'stop' : 'start';

  try {
    const formData = new URLSearchParams();
    formData.append('action', action);
    if (action === 'start') {
      formData.append('targetTemp', isNaN(targetTemp) ? '20.0' : targetTemp.toString());
      formData.append('testPWM', isNaN(testPwm) ? '300' : testPwm.toString());
    }

    const res = await fetch('/api/autotune', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: formData.toString()
    });
    if (res.ok) {
      pollAutotuneStatus();
    }
  } catch (e) {
    alert('Error sending autotune request: ' + e);
  }
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

    initAutotuneTarget(data.targetTemp || data.setTemp);
    initAutotunePwm(300);

    pollAutotuneStatus();
    if (!autotuneTimer) {
      autotuneTimer = setInterval(pollAutotuneStatus, 2000);
    }
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

    const setIf = (id, val) => {
      const el = document.getElementById(id);
      if (el && val !== undefined) el.textContent = val;
    };

    setIf('Caption', data.Caption);
    setIf('freeHeap', data.freeHeap);
    setIf('wifiRssi', data.wifiRssi);
    setIf('lastWifiDrop', data.lastWifiDrop);
    setIf('i2cStatus', data.i2cStatus);
    setIf('resetReason', data.resetReason);
    setIf('sensorCount', data.sensorCount);
    setIf('sensorTemps', data.sensorTemps || data.sensorCount);
    setIf('dbgUptime', data.uptime);

    const debugLog = document.getElementById('debugLog');
    if (debugLog && data.debug !== undefined) debugLog.innerHTML = data.debug;
  } catch (e) {}
}

function setupDebugPage() {
  fetchDebug();
  setInterval(() => {
    if (!document.hidden) fetchDebug();
  }, 5000);

  const refreshBtn = document.getElementById('refreshDebugBtn');
  if (refreshBtn) {
    refreshBtn.addEventListener('click', (e) => {
      e.preventDefault();
      fetchDebug();
    });
  }
}

document.addEventListener('DOMContentLoaded', () => {
  setupNavigation();
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
