async function fetchAndUpdateWirelessStatus() {
    const statusEls = document.querySelectorAll('.wireless-status-container');
    if (!statusEls.length) return;

    // Set initial loading state
    statusEls.forEach(container => {
        container.innerHTML = `
            <div style="display: flex; gap: 20px; font-size: 13px; color: var(--text-gray);">
                <div>
                    <strong style="color: var(--text-light);">NodeMCU</strong><br>
                    <span style="color: #fbbf24;">🟡 Checking...</span>
                </div>
                <div>
                    <strong style="color: var(--text-light);">Network</strong><br>
                    <span style="color: #fbbf24;">🟡 Checking...</span>
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
                <strong style="color: var(--text-light);">NodeMCU</strong><br>
                <span style="color: var(--status-green);">🟢 ONLINE</span><br>
                IP: ${nodemcu.ip}
            `;
        } else {
            nodemcuHtml = `
                <strong style="color: var(--text-light);">NodeMCU</strong><br>
                <span style="color: var(--status-red);">🔴 OFFLINE</span>
            `;
        }
        
        let networkHtml = '';
        if (network.status === 'connected') {
            networkHtml = `
                <strong style="color: var(--text-light);">Network</strong><br>
                <span style="color: var(--status-green);">🟢 CONNECTED</span><br>
                ${network.ssid ? 'Wi-Fi: ' + network.ssid + '<br>' : ''}
                Laptop IP: ${network.ip || 'Unknown'}
            `;
        } else {
            networkHtml = `
                <strong style="color: var(--text-light);">Network</strong><br>
                <span style="color: var(--status-red);">🔴 DISCONNECTED</span>
            `;
        }

        statusEls.forEach(container => {
            container.innerHTML = `
                <div style="display: flex; gap: 30px; font-size: 13px; color: var(--text-gray); align-items: flex-start;">
                    <div>${nodemcuHtml}</div>
                    <div>${networkHtml}</div>
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
