import re

with open('dashboard_local.html', 'r') as f:
    content = f.read()

# Fix default settings in HTML
content = content.replace('value="8083"', 'value="8084"')
content = content.replace('value="DIESEL001"', 'value="default_esp32_01"')

# Fix loadSettings defaults
content = content.replace("s.port || 8083", "s.port || 8084")
content = content.replace("s.deviceId || 'DIESEL001'", "s.deviceId || 'default_esp32_01'")
content = content.replace("getElementById('cfgPort').value) || 8083", "getElementById('cfgPort').value) || 8084")

# Fix connectMQTT topics and WSS
content = content.replace('`ws://${cfg.broker}:${cfg.port}/mqtt`', '`wss://${cfg.broker}:${cfg.port}/mqtt`')
content = content.replace('`${cfg.prefix}/${cfg.deviceId}/data`', '`${cfg.prefix}/${cfg.deviceId}/telemetry/flow`')

# Fix subscription to include status
sub_old = """    client.on('connect', () => {
      setConnected(true);
      client.subscribe(dataTopic, { qos: 0 });
      console.log('Subscribed to', dataTopic);
    });"""

sub_new = """    client.on('connect', () => {
      setConnected(true);
      client.subscribe(dataTopic, { qos: 0 });
      client.subscribe(`${cfg.prefix}/${cfg.deviceId}/status`, { qos: 0 });
      console.log('Subscribed to', dataTopic);
    });"""
content = content.replace(sub_old, sub_new)

# Fix message handler
msg_old = """    client.on('message', (topic, message) => {
      try {
        const d = JSON.parse(message.toString());

        // Primary flow metrics
        animateValue('freq', d.frequency_hz, 1);
        animateValue('flow', d.flow_rate, 2);
        animateValue('total', d.total_liters, 2);

        // Modbus extended metrics
        if (d.temperature !== undefined) animateValue('temp', d.temperature, 1);
        if (d.velocity !== undefined) animateValue('velocity', d.velocity, 2);
        if (d.flow_m3h !== undefined) animateValue('flowm3', d.flow_m3h, 3);
        if (d.cumulative_m3 !== undefined) animateValue('cumul', d.cumulative_m3, 3);

        // Modbus connection status
        if (d.modbus_online !== undefined) setModbusStatus(d.modbus_online);

        // Delivery count
        document.getElementById('delCount').textContent = d.delivery_count ?? '--';

        // Duration
        if (d.delivery_duration !== undefined) {
          const m = Math.floor(d.delivery_duration / 60);
          const s = d.delivery_duration % 60;
          document.getElementById('duration').textContent =
            String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
        }

        // Active delivery liters
        const info = document.getElementById('deliveryInfo');
        if (d.delivery_state === 'RUNNING') {
          info.classList.add('visible');
          document.getElementById('delLiters').textContent =
            parseFloat(d.delivery_liters || 0).toFixed(2);
        } else {
          info.classList.remove('visible');
        }

        // State badge
        updateState(d.delivery_state || 'IDLE');

        // Footer info
        document.getElementById('devId').textContent = d.device_id || '--';
        document.getElementById('timestamp').textContent = d.timestamp || '--';

      } catch (err) { /* ignore */ }
    });"""

msg_new = """    client.on('message', (topic, message) => {
      try {
        const d = JSON.parse(message.toString());
        
        if (topic === dataTopic) {
            animateValue('flow', d.flow_rate, 2);
            animateValue('total', d.total_vol, 2);
            document.getElementById('timestamp').textContent = 'Time: ' + (d.ts || '--') + ' | Seq: ' + (d.seq || '--');
            document.getElementById('devId').textContent = document.getElementById('cfgDeviceId').value;
        } else if (topic.endsWith('/status')) {
            if (d.status) {
                setModbusStatus(d.status === 'online');
            }
        }
      } catch (err) { /* ignore */ }
    });"""

content = content.replace(msg_old, msg_new)

with open('dashboard_local.html', 'w') as f:
    f.write(content)

