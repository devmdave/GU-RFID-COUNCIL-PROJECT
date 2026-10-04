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
                showModal(data.scan, data.blocked);
                if (!data.blocked) {
                    addToActivityTable(data.scan);
                }
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
    let accessBadge = 'badge-denied';
    if (data.access === 'EXIT') accessBadge = 'badge-exit';
    if (data.access === 'ENTRY') accessBadge = 'badge-entry';
    
    let attBadge = 'badge-denied';
    if (data.attendance === 'COUNTED') attBadge = 'badge-counted';
    if (data.attendance === 'PENDING') attBadge = 'badge-pending';
    
    const tr = document.createElement('tr');
    tr.innerHTML = `
        <td class="font-space">${data.name || '-'}</td>
        <td class="font-space">${data.enrollment || '-'}</td>
        <td>${data.role || '-'}</td>
        <td>${data.committee || '-'}</td>
        <td>${data.timestamp || '-'}</td>
        <td><span class="badge ${accessBadge}">${data.access || '-'}</span></td>
        <td><span class="badge ${attBadge}">${data.attendance || '-'}</span></td>
    `;
    
    activityTbody.insertBefore(tr, activityTbody.firstChild);
    
    if (activityTbody.children.length > 15) {
        activityTbody.removeChild(activityTbody.lastChild);
    }
}

function showModal(data, isBlocked = false) {
    clearTimeout(modalTimeout);
    
    const isGranted = !isBlocked && data.access !== 'denied';
    
    // Reset classes
    modalCard.className = 'modal-card glass-panel';
    if (isBlocked) {
        modalCard.classList.add('modal-denied'); // use denied color/styles
    } else if (isGranted) {
        modalCard.classList.add('modal-granted');
    } else {
        modalCard.classList.add('modal-denied');
    }
    
    // Update content
    modalCardNumber.textContent = data.name ? `${data.name} (${data.number})` : data.number;
    
    if (isBlocked) {
        modalTitle.textContent = 'ATTENDANCE NOT COUNTED';
        modalMessage.innerHTML = 'Minimum 15 minutes are required between<br>Entry and Exit.';
        modalIcon.setAttribute('data-lucide', 'x-circle');
        modalBadge.innerHTML = '<i data-lucide="x"></i> BLOCKED';
    } else if (isGranted) {
        if (data.access === 'EXIT') {
            modalTitle.textContent = 'VALID EXIT';
            modalMessage.textContent = 'Access has been logged as EXIT.';
        } else {
            modalTitle.textContent = 'VALID ENTRY';
            modalMessage.textContent = 'Access has been logged as ENTRY.';
        }
        modalIcon.setAttribute('data-lucide', 'check-circle');
        modalBadge.innerHTML = `<i data-lucide="check"></i> ${data.attendance || 'VERIFIED'}`;
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
    
    // Auto close after 4 seconds
    modalTimeout = setTimeout(() => {
        closeModal();
    }, 4000);
}

function closeModal() {
    modalOverlay.classList.remove('active');
}

modalClose.addEventListener('click', closeModal);
