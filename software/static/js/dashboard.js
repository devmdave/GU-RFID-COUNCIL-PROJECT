let lastScanId = 0;
let modalTimeout;

const modalOverlay = document.getElementById('access-modal');
const modalCard = document.getElementById('modal-content');
const modalClose = document.getElementById('modal-close');
const modalIcon = document.getElementById('modal-icon');
const modalTitle = document.getElementById('modal-title');
const modalCardNumber = document.getElementById('modal-card-number');
const modalMessage = document.getElementById('modal-message');
const modalBadge = document.getElementById('modal-badge');

const lastScanContent = document.getElementById('last-scan-content');
const activityTbody = document.getElementById('activity-tbody');

// Initialize polling
document.addEventListener('DOMContentLoaded', () => {
    startPolling();
});

function startPolling() {
    setInterval(fetchLatestScan, 1000);
}

async function fetchLatestScan() {
    try {
        const response = await fetch('/latest-scan');
        if (!response.ok) {
            if (response.status === 401) {
                window.location.href = '/login';
            }
            return;
        }
        
        const data = await response.json();
        
        // If there's a new scan
        if (data.id && data.id > lastScanId) {
            lastScanId = data.id;
            handleNewScan(data);
        }
    } catch (error) {
        console.error("Error fetching latest scan:", error);
    }
}

function handleNewScan(data) {
    updateLastScan(data);
    addToActivityTable(data);
    showModal(data);
}

function updateLastScan(data) {
    const isGranted = data.access === 'granted';
    const statusClass = isGranted ? 'text-green' : 'text-red';
    const statusText = isGranted ? 'GRANTED' : 'DENIED';
    
    lastScanContent.innerHTML = `
        <div class="scan-details-block">
            <p class="text-gray text-sm m-0">Card Number:</p>
            <p class="font-space text-lg m-0 mt-1">${data.number}</p>
        </div>
        <div class="scan-details-block">
            <p class="text-gray text-sm m-0">Status:</p>
            <p class="font-space text-lg m-0 mt-1 ${statusClass}">${statusText}</p>
        </div>
        <div class="scan-details-block">
            <p class="text-gray text-sm m-0">Time:</p>
            <p class="font-space text-lg m-0 mt-1">${data.timestamp}</p>
        </div>
    `;
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
