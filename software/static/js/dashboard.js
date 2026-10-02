let modalTimeout;

const modalOverlay = document.getElementById('access-modal');
const modalCard = document.getElementById('modal-content');
const modalClose = document.getElementById('modal-close');
const modalIcon = document.getElementById('modal-icon');
const modalTitle = document.getElementById('modal-title');
const modalCardNumber = document.getElementById('modal-card-number');
const modalMessage = document.getElementById('modal-message');
const modalBadge = document.getElementById('modal-badge');

const activityTbody = document.getElementById('activity-tbody');

document.addEventListener('DOMContentLoaded', () => {
    fetchAccessLogsOnce();
    initSSE();
});

function initSSE() {
    const eventSource = new EventSource('/api/scan-events');
    
    eventSource.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            if (data.success && data.scan) {
                showModal(data.scan);
            }
        } catch (e) {
            console.error("Error parsing SSE data", e);
        }
    };

    eventSource.onerror = function(err) {
        console.error("EventSource failed.", err);
    };
}

async function fetchAccessLogsOnce() {
    try {
        const response = await fetch('/api/access-logs');
        if (!response.ok) return;
        const data = await response.json();
        
        if (data.success && data.logs && data.logs.length > 0) {
            // Reverse so oldest are inserted first, ending up properly ordered at top
            data.logs.reverse().forEach(log => {
                addToActivityTable(log);
            });
        }
    } catch (e) {
        console.error("Error fetching access logs:", e);
    }
}

function addToActivityTable(data) {
    const isGranted = data.access === 'granted';
    const badgeClass = isGranted ? 'badge-granted' : 'badge-denied';
    const statusText = isGranted ? 'GRANTED' : 'DENIED';
    
    const tr = document.createElement('tr');
    tr.innerHTML = `
        <td class="font-space">${data.number}</td>
        <td><span class="badge ${badgeClass}">${statusText}</span></td>
        <td>${data.timestamp}</td>
    `;
    
    // Insert at top
    activityTbody.insertBefore(tr, activityTbody.firstChild);
    
    // Keep only last 10 rows
    if (activityTbody.children.length > 10) {
        activityTbody.removeChild(activityTbody.lastChild);
    }
}

function showModal(data) {
    clearTimeout(modalTimeout);
    
    const isGranted = data.access === 'granted';
    
    // Reset classes
    modalCard.className = 'modal-card glass-panel';
    modalCard.classList.add(isGranted ? 'modal-granted' : 'modal-denied');
    
    // Update content
    modalCardNumber.textContent = data.number;
    
    if (isGranted) {
        modalTitle.textContent = 'ACCESS GRANTED';
        modalMessage.textContent = 'Access has been granted.';
        modalIcon.setAttribute('data-lucide', 'check-circle');
        modalBadge.innerHTML = '<i data-lucide="check"></i> VERIFIED';
    } else {
        modalTitle.textContent = 'ACCESS DENIED';
        modalMessage.textContent = 'Access has been denied.';
        modalIcon.setAttribute('data-lucide', 'x-circle');
        modalBadge.innerHTML = '<i data-lucide="x"></i> NOT AUTHORIZED';
    }
    
    // Refresh icons inside modal
    lucide.createIcons();
    
    // Show modal
    modalOverlay.classList.add('active');
    
    // Auto close after 3 seconds
    modalTimeout = setTimeout(() => {
        closeModal();
    }, 3000);
}

function closeModal() {
    modalOverlay.classList.remove('active');
}

modalClose.addEventListener('click', closeModal);
