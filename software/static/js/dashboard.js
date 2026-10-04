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

// --- Add User / Card Writing Logic ---
const btnAddUser = document.getElementById('btn-add-user');
const addUserModal = document.getElementById('add-user-modal');
const addUserForm = document.getElementById('add-user-form');
const writingModal = document.getElementById('writing-modal');
const btnCancelWrite = document.getElementById('btn-cancel-write');

let serialEventSource = null;
let currentPendingNumber = null;

if (btnAddUser) {
    btnAddUser.addEventListener('click', () => {
        addUserModal.classList.add('active');
    });
}

if (addUserForm) {
    addUserForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        const name = document.getElementById('new-name').value;
        const enrollment = document.getElementById('new-enrollment').value;
        const role = document.getElementById('new-role').value;
        const committee = document.getElementById('new-committee').value;
        
        try {
            const res = await fetch('/api/cards/new', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, enrollment, role, committee })
            });
            const data = await res.json();
            
            if (data.success) {
                addUserModal.classList.remove('active');
                currentPendingNumber = data.number;
                
                // Set modal details
                document.getElementById('write-user-name').textContent = name;
                document.getElementById('write-user-enrollment').textContent = enrollment;
                document.getElementById('write-card-number').textContent = currentPendingNumber;
                
                // Reset status UI
                document.getElementById('write-status-text').innerHTML = '🟡 Waiting for card...<br><span class="text-gray text-sm">Please place the NFC card on the RC522 reader.</span>';
                document.getElementById('write-status-icon').innerHTML = '<i data-lucide="loader" class="spin text-cyan" style="width: 48px; height: 48px;"></i>';
                btnCancelWrite.style.display = 'block';
                
                lucide.createIcons();
                writingModal.classList.add('active');
                
                startSerialStream();
            } else {
                alert('Error: ' + data.error);
            }
        } catch (err) {
            console.error(err);
            alert('Error creating pending card.');
        }
    });
}

function startSerialStream() {
    if (serialEventSource) {
        serialEventSource.close();
    }
    serialEventSource = new EventSource('/api/device/serial-stream');
    
    serialEventSource.onmessage = async function(event) {
        try {
            const data = JSON.parse(event.data);
            if (data.type === 'serial' && data.line) {
                const line = data.line.trim();
                
                if (line === 'CARD_DETECTED') {
                    document.getElementById('write-status-text').textContent = 'Card detected. Writing card...';
                } else if (line === 'WRITING_CARD') {
                    document.getElementById('write-status-text').textContent = 'Writing card...';
                } else if (line === 'WRITE_SUCCESS') {
                    document.getElementById('write-status-text').innerHTML = `✅ Card Written Successfully<br><br>Name: ${document.getElementById('write-user-name').textContent}<br>Enrollment: ${document.getElementById('write-user-enrollment').textContent}<br>Card Number: ${currentPendingNumber}`;
                    document.getElementById('write-status-icon').innerHTML = '<i data-lucide="check-circle" class="text-green" style="width: 48px; height: 48px;"></i>';
                    btnCancelWrite.style.display = 'none';
                    lucide.createIcons();
                    
                    // Activate in DB
                    try {
                        await fetch('/api/cards/activate', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ number: currentPendingNumber })
                        });
                    } catch (e) { console.error(e); }
                    
                    setTimeout(() => {
                        writingModal.classList.remove('active');
                        if (serialEventSource) serialEventSource.close();
                        addUserForm.reset();
                    }, 4000);
                    
                } else if (line === 'WRITE_FAILED') {
                    document.getElementById('write-status-text').innerHTML = '❌ Card Writing Failed<br><span class="text-gray text-sm">Please try again.</span>';
                    document.getElementById('write-status-icon').innerHTML = '<i data-lucide="x-circle" class="text-red" style="width: 48px; height: 48px; color: var(--status-red);"></i>';
                    lucide.createIcons();
                    
                    try {
                        await fetch('/api/cards/cancel', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify({ number: currentPendingNumber })
                        });
                    } catch (e) {}
                    
                    setTimeout(() => {
                        writingModal.classList.remove('active');
                        if (serialEventSource) serialEventSource.close();
                    }, 3000);
                }
            }
        } catch(e) {}
    };
}

if (btnCancelWrite) {
    btnCancelWrite.addEventListener('click', async () => {
        try {
            await fetch('/api/cards/cancel', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ number: currentPendingNumber })
            });
        } catch (e) {}
        
        writingModal.classList.remove('active');
        if (serialEventSource) {
            serialEventSource.close();
        }
    });
}
