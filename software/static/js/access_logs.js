document.addEventListener('DOMContentLoaded', () => {
    const dateFilter = document.getElementById('date-filter');
    const btnClearFilter = document.getElementById('btn-clear-filter');
    const btnExport = document.getElementById('btn-export');
    const filterStatusText = document.getElementById('filter-status-text');
    const logsTbody = document.getElementById('logs-tbody');

    function fetchLogs() {
        // Set loading state before fetching
        logsTbody.innerHTML = `
            <tr>
                <td colspan="7" style="text-align: center; padding: 40px;">
                    <div style="display: flex; flex-direction: column; align-items: center; justify-content: center; color: var(--text-muted); gap: 12px;">
                        <span style="font-size: 14px; font-weight: 500;">Loading access logs...</span>
                    </div>
                </td>
            </tr>
        `;
        
        const dateVal = dateFilter.value;
        let url = '/api/access-logs?limit=all';
        
        if (dateVal) {
            url += `&date=${dateVal}`;
            const d = new Date(dateVal);
            const dateStr = d.toLocaleDateString('en-GB', { day: '2-digit', month: 'long', year: 'numeric' });
            filterStatusText.textContent = `Showing logs for: ${dateStr}`;
        } else {
            filterStatusText.textContent = 'Showing: All logs';
        }

        fetch(url)
            .then(res => res.json())
            .then(data => {
                if (data.success) {
                    renderLogs(data.logs);
                }
            })
            .catch(err => {
                console.error('Error fetching logs:', err);
                logsTbody.innerHTML = `
                    <tr>
                        <td colspan="7" style="text-align: center; padding: 60px;">
                            <div style="display: flex; flex-direction: column; align-items: center; justify-content: center; color: var(--status-red); gap: 16px;">
                                <i data-lucide="alert-triangle" style="width: 48px; height: 48px; opacity: 0.8;"></i>
                                <span style="font-size: 15px; font-weight: 500;">Error loading logs. Please try again.</span>
                            </div>
                        </td>
                    </tr>
                `;
                if (window.lucide) lucide.createIcons();
            });
    }

    function renderLogs(logs) {
        logsTbody.innerHTML = '';
        if (logs.length === 0) {
            logsTbody.innerHTML = `
                <tr>
                    <td colspan="7" style="text-align: center; padding: 60px;">
                        <div style="display: flex; flex-direction: column; align-items: center; justify-content: center; color: var(--text-muted); gap: 16px;">
                            <i data-lucide="file-search" style="width: 48px; height: 48px; opacity: 0.5;"></i>
                            <span style="font-size: 15px; font-weight: 500;">No access logs found</span>
                        </div>
                    </td>
                </tr>
            `;
            if (window.lucide) lucide.createIcons();
            return;
        }

        logs.forEach(data => {
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
            logsTbody.appendChild(tr);
        });
    }

    dateFilter.addEventListener('change', () => {
        fetchLogs();
    });

    btnClearFilter.addEventListener('click', () => {
        dateFilter.value = '';
        fetchLogs();
    });

    btnExport.addEventListener('click', () => {
        const dateVal = dateFilter.value;
        let url = '/api/access-logs/export';
        if (dateVal) {
            url += `?date=${dateVal}`;
        }
        window.location.href = url;
    });

    // Initial load
    fetchLogs();
});
