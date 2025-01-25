export function showStatus(message, type = '', duration = 3000) {
    const status = document.getElementById('status');
    if (!status) {
        console.warn('Status element not found');
        return;
    }

    status.textContent = message;
    status.className = `status ${type} visible`;

    if (duration) {
        setTimeout(() => {
            status.className = 'status';
        }, duration);
    }
}

function formatTimestamp(epoch) {
    const date = new Date(epoch * 1000);
    return date.toLocaleString('ru-RU');
}

function formatUptime(seconds) {
    const hrs = Math.floor(seconds / 3600);
    const mins = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    return `${hrs}h ${mins}m ${secs}s`;
}

export { formatTimestamp, formatUptime };
