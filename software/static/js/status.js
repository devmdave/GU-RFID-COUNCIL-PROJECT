async function fetchAndUpdateWirelessStatus() {
    const statusEls = document.querySelectorAll('.wireless-status-container');
    if (!statusEls.length) return;

    // Set initial loading state
    statusEls.forEach(container => {
        container.innerHTML = `
            <div class="wireless-status-grid">
                <div class="wireless-status-item">
                    <div class="status-label">NODEMCU</div>
                    <div class="status-indicator" style="color: #fbbf24;">🟡 Checking...</div>
                </div>
                <div class="wireless-status-item">
                    <div class="status-label">NETWORK</div>
                    <div class="status-indicator" style="color: #fbbf24;">🟡 Checking...</div>
                </div>
            </div>
        `;
    });

    try {
        const response = await fetch('/api/device/status');
        const data = await response.json();
        
        const nodemcu = data.nodemcu || {};
        const network = data.network || {};
        
        let nodemcuHtml = '';
        if (nodemcu.status === 'online') {
            nodemcuHtml = `
                <div class="status-label">NODEMCU</div>
                <div class="status-indicator text-green">🟢 ONLINE</div>
                <div class="status-detail">IP: ${nodemcu.ip}</div>
            `;
        } else {
            nodemcuHtml = `
                <div class="status-label">NODEMCU</div>
                <div class="status-indicator text-red">🔴 OFFLINE</div>
            `;
        }
        
        let networkHtml = '';
        if (network.status === 'connected') {
            networkHtml = `
                <div class="status-label">NETWORK</div>
                <div class="status-indicator text-green">🟢 CONNECTED</div>
                <div class="status-detail">${network.ssid ? 'Wi-Fi: ' + network.ssid + '<br>' : ''}Laptop IP: ${network.ip || 'Unknown'}</div>
            `;
        } else {
            networkHtml = `
                <div class="status-label">NETWORK</div>
                <div class="status-indicator text-red">🔴 DISCONNECTED</div>
            `;
        }

        statusEls.forEach(container => {
            container.innerHTML = `
                <div class="wireless-status-grid">
                    <div class="wireless-status-item">${nodemcuHtml}</div>
                    <div class="wireless-status-item">${networkHtml}</div>
                </div>
            `;
        });
        
        // Also update navbar if exists
        const navStatus = document.querySelector('.nav-status-indicator');
        if (navStatus) {
            if (nodemcu.status === 'online') {
                navStatus.innerHTML = `<span class="pulse-dot" style="background-color: var(--status-green);"></span> NodeMCU Online`;
            } else {
                navStatus.innerHTML = `<span class="pulse-dot" style="background-color: var(--status-red);"></span> NodeMCU Offline`;
            }
        }
        
    } catch (error) {
        console.error('Error fetching wireless status:', error);
        statusEls.forEach(container => {
            container.innerHTML = `
                <div style="color: var(--status-red); font-size: 13px;">
                    🔴 Failed to load status
                </div>
            `;
        });
    }
}

document.addEventListener('DOMContentLoaded', fetchAndUpdateWirelessStatus);
