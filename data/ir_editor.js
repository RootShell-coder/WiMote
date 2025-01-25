let isLearning = false;

async function loadCommands() {
    try {
        // Проверяем статус WiFi перед загрузкой команд
        const statusResponse = await fetch('/status');
        const statusData = await statusResponse.json();

        if (statusData.wifi.status !== 'connected') {
            // Если WiFi не подключен, редиректим на страницу настройки
            window.location.href = '/config.html';
            return;
        }

        const response = await fetch('/ir/commands');
        const data = await response.json();
        renderCommands(data.buttons);
    } catch (error) {
        showStatus('Failed to load commands. Check WiFi connection.', 'error');
        console.error('Load error:', error);
    }
}

function renderCommands(commands) {
    const container = document.getElementById('commands-list');
    container.innerHTML = '';

    Object.entries(commands).forEach(([key, command]) => {
        const item = document.createElement('div');
        item.className = 'command-item';
        item.innerHTML = `
            <header>
                <strong class="command-name">${key}</strong>
                <div class="button-group">
                    <button onclick="deleteCommand('${key}')" class="delete-btn">Delete</button>
                    <button onclick="editCommand('${key}')" class="edit-btn">Edit</button>
                </div>
            </header>
            <div class="command-details">
                <div class="input-group">
                    <label>Protocol:</label>
                    <input type="text" class="command-protocol" value="${command.protocol}" readonly>
                </div>
                <div class="input-group">
                    <label>Code:</label>
                    <input type="text" class="command-code" value="${command.code}" readonly>
                </div>
                <div class="input-group">
                    <label>Bits:</label>
                    <input type="number" class="command-bits" value="${command.bits}" readonly>
                </div>
                <div class="input-group">
                    <label>Description:</label>
                    <input type="text" class="command-description" value="${command.description || ''}" readonly>
                </div>
            </div>
        `;
        container.appendChild(item);
    });
}

async function editCommand(key) {
    try {
        const response = await fetch('/ir/commands');
        const data = await response.json();
        const command = data.buttons[key];

        const result = await showEditDialog(key, command);
        if (!result) return;

        data.buttons[key] = result;

        const saveResponse = await fetch('/ir/commands/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (saveResponse.ok) {
            showStatus('Command updated successfully', 'success');
            loadCommands();
        } else {
            throw new Error('Failed to save command');
        }
    } catch (error) {
        showStatus('Error updating command', 'error');
    }
}

function showDeleteConfirmDialog(key) {
    return new Promise((resolve) => {
        const modal = document.createElement('div');
        modal.className = 'modal';
        modal.innerHTML = `
            <div class="modal-content">
                <h3>Delete Command</h3>
                <p>Are you sure you want to delete "${key}" command?</p>
                <div class="button-group">
                    <button id="confirm-btn" class="delete-btn">Delete</button>
                    <button id="cancel-btn">Cancel</button>
                </div>
            </div>
        `;

        document.body.appendChild(modal);

        const confirmBtn = modal.querySelector('#confirm-btn');
        const cancelBtn = modal.querySelector('#cancel-btn');

        confirmBtn.onclick = () => {
            document.body.removeChild(modal);
            resolve(true);
        };

        cancelBtn.onclick = () => {
            document.body.removeChild(modal);
            resolve(false);
        };

        // Обработка клавиш
        modal.addEventListener('keyup', (e) => {
            if (e.key === 'Enter') confirmBtn.click();
            if (e.key === 'Escape') cancelBtn.click();
        });
    });
}

async function deleteCommand(key) {
    const shouldDelete = await showDeleteConfirmDialog(key);
    if (!shouldDelete) return;

    try {
        const response = await fetch('/ir/commands');
        const data = await response.json();

        delete data.buttons[key];

        const saveResponse = await fetch('/ir/commands/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });

        if (saveResponse.ok) {
            showStatus('Command deleted successfully', 'success');
            loadCommands();
        } else {
            throw new Error('Failed to delete command');
        }
    } catch (error) {
        showStatus('Error deleting command', 'error');
    }
}

function showEditDialog(key, command) {
    return new Promise((resolve) => {
        const modal = document.createElement('div');
        modal.className = 'modal';
        modal.innerHTML = `
            <div class="modal-content">
                <h3>Edit Command: ${key}</h3>
                <div class="input-group">
                    <label>Name:</label>
                    <input type="text" id="edit-name" value="${key}">
                </div>
                <div class="input-group">
                    <label>Protocol:</label>
                    <input type="text" id="edit-protocol" value="${command.protocol}">
                </div>
                <div class="input-group">
                    <label>Code:</label>
                    <input type="text" id="edit-code" value="${command.code}">
                </div>
                <div class="input-group">
                    <label>Bits:</label>
                    <input type="number" id="edit-bits" value="${command.bits}">
                </div>
                <div class="input-group">
                    <label>Description:</label>
                    <input type="text" id="edit-description" value="${command.description || ''}">
                </div>
                <div class="button-group">
                    <button id="save-btn">Save</button>
                    <button id="cancel-btn">Cancel</button>
                </div>
            </div>
        `;

        document.body.appendChild(modal);

        const saveBtn = modal.querySelector('#save-btn');

        const cancelBtn = modal.querySelector('#cancel-btn');

        saveBtn.onclick = () => {
            const newCommand = {
                protocol: modal.querySelector('#edit-protocol').value,
                code: modal.querySelector('#edit-code').value,
                bits: parseInt(modal.querySelector('#edit-bits').value),
                description: modal.querySelector('#edit-description').value
            };
            document.body.removeChild(modal);
            resolve(newCommand);
        };

        cancelBtn.onclick = () => {
            document.body.removeChild(modal);
            resolve(null);
        };
    });
}

function showNameDialog(learningResult) {
    return new Promise((resolve) => {
        const modal = document.createElement('div');
        modal.className = 'modal';
        modal.innerHTML = `
            <div class="modal-content">
                <h3>New IR Command Received</h3>
                <div class="command-info">
                    <div class="input-group">
                        <label>Protocol:</label>
                        <input type="text" value="${learningResult.protocol}" readonly>
                    </div>
                    <div class="input-group">
                        <label>Code:</label>
                        <input type="text" value="${learningResult.code}" readonly>
                    </div>
                    <div class="input-group">
                        <label>Bits:</label>
                        <input type="number" value="${learningResult.bits}" readonly>
                    </div>
                </div>
                <div class="input-group">
                    <label>Choose a name for this command:</label>
                    <select id="command-name">
                        <option value="POWER">Power</option>
                        <option value="VOL_UP">Volume Up</option>
                        <option value="VOL_DOWN">Volume Down</option>
                        <option value="CH_UP">Channel Up</option>
                        <option value="CH_DOWN">Channel Down</option>
                        <option value="MUTE">Mute</option>
                        <option value="MENU">Menu</option>
                        <option value="CUSTOM">Custom Name...</option>
                    </select>
                    <input type="text" id="custom-name" placeholder="Enter custom name" style="display: none;">
                </div>
                <div class="input-group">
                    <label>Description:</label>
                    <input type="text" id="command-description" placeholder="Enter description">
                </div>
                <div class="button-group">
                    <button id="save-btn" class="remote-button">Save Command</button>
                    <button id="cancel-btn">Cancel</button>
                </div>
            </div>
        `;

        document.body.appendChild(modal);

        const select = modal.querySelector('#command-name');
        const customInput = modal.querySelector('#custom-name');
        const saveBtn = modal.querySelector('#save-btn');
        const cancelBtn = modal.querySelector('#cancel-btn');
        const descriptionInput = modal.querySelector('#command-description');

        // Показываем поле для ввода при выборе "Custom Name"
        select.addEventListener('change', () => {
            if (select.value === 'CUSTOM') {
                customInput.style.display = 'block';
                customInput.focus();
            } else {
                customInput.style.display = 'none';
            }
        });

        saveBtn.onclick = () => {
            const commandName = select.value === 'CUSTOM' ? customInput.value : select.value;
            if (!commandName.trim()) {
                alert('Please enter a command name');
                return;
            }
            document.body.removeChild(modal);
            resolve({
                name: commandName,
                description: descriptionInput.value
            });
        };

        cancelBtn.onclick = () => {
            document.body.removeChild(modal);
            resolve(null);
        };

        // Обработка клавиши Enter
        modal.addEventListener('keyup', (e) => {
            if (e.key === 'Enter') saveBtn.click();
            if (e.key === 'Escape') cancelBtn.click();
        });
    });
}

async function startLearning() {
    if (isLearning) return;

    const button = document.querySelector('.learn-button');
    button.classList.add('learning');
    button.textContent = 'Learning... Press IR button';
    button.disabled = true;
    isLearning = true;

    try {
        const response = await fetch('/ir/learn', { method: 'POST' });
        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }

        let learningResult = null;
        const maxAttempts = 100;
        let attempts = 0;

        while (!learningResult && attempts < maxAttempts) {
            await new Promise(resolve => setTimeout(resolve, 100));
            attempts++;

            const statusResponse = await fetch('/ir/learn/status');
            const statusData = await statusResponse.json();

            if (statusData.completed) {
                if (statusData.error) {
                    throw new Error(statusData.error);
                }
                if (statusData.result) {
                    learningResult = statusData.result;
                    console.log('Learned:', learningResult);
                    break;
                }
            }
        }

        if (!learningResult) {
            throw new Error('No IR signal received. Try again.');
        }

        const nameResult = await showNameDialog(learningResult);
        if (nameResult) {
            const commands = await (await fetch('/ir/commands')).json();
            commands.buttons[nameResult.name] = {
                protocol: learningResult.protocol,
                code: learningResult.code,
                bits: learningResult.bits,
                description: nameResult.description || nameResult.name
            };

            const saveResponse = await fetch('/ir/commands/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(commands)
            });

            if (!saveResponse.ok) {
                throw new Error('Failed to save command');
            }

            showStatus('New command learned and saved', 'success');
            loadCommands();
        }
    } catch (error) {
        console.error('Learning error:', error);
        showStatus(error.message || 'Learning failed', 'error');
    } finally {
        button.classList.remove('learning');
        button.textContent = 'Learn New Command';
        button.disabled = false;
        isLearning = false;
    }
}

function showManualAddDialog() {
    const protocols = [
        "NEC", "SAMSUNG", "SONY", "LG", "RC5", "RC6", "PANASONIC",
        "MITSUBISHI", "DENON", "SHARP", "JVC", "SANYO", "DISH",
        "WHYNTER", "COOLIX", "GREE", "HITACHI_AC", "KELVINATOR",
        "CARRIER_AC", "DAIKIN", "MIDEA", "HAIER_AC"
    ];

    const modal = document.createElement('div');
    modal.className = 'modal';
    modal.innerHTML = `
        <div class="modal-content">
            <h3>Add Command Manually</h3>
            <div class="input-group">
                <label>Name:</label>
                <select id="command-name">
                    <option value="POWER">Power</option>
                    <option value="VOL_UP">Volume Up</option>
                    <option value="VOL_DOWN">Volume Down</option>
                    <option value="CH_UP">Channel Up</option>
                    <option value="CH_DOWN">Channel Down</option>
                    <option value="MUTE">Mute</option>
                    <option value="MENU">Menu</option>
                    <option value="CUSTOM">Custom Name...</option>
                </select>
                <input type="text" id="custom-name" placeholder="Enter custom name" style="display: none;">
            </div>
            <div class="input-group">
                <label>Protocol:</label>
                <select id="manual-protocol">
                    ${protocols.map(p => `<option value="${p}">${p}</option>`).join('')}
                </select>
            </div>
            <div class="input-group">
                <label>Code (hex):</label>
                <input type="text" id="manual-code" placeholder="e.g., 0x20DF10EF">
            </div>
            <div class="input-group">
                <label>Bits:</label>
                <input type="number" id="manual-bits" value="32">
            </div>
            <div class="input-group">
                <label>Description:</label>
                <input type="text" id="manual-description" placeholder="Enter description">
            </div>
            <div class="button-group">
                <button id="save-btn" class="remote-button">Save Command</button>
                <button id="cancel-btn">Cancel</button>
            </div>
        </div>
    `;

    document.body.appendChild(modal);

    const select = modal.querySelector('#command-name');

    const customInput = modal.querySelector('#custom-name');
    const saveBtn = modal.querySelector('#save-btn');
    const cancelBtn = modal.querySelector('#cancel-btn');
    const codeInput = modal.querySelector('#manual-code');

    select.addEventListener('change', () => {
        if (select.value === 'CUSTOM') {
            customInput.style.display = 'block';
            customInput.focus();
        } else {
            customInput.style.display = 'none';
        }
    });

    // Автоматическое форматирование кода в hex формат
    codeInput.addEventListener('input', (e) => {
        let value = e.target.value.replace(/[^0-9a-fA-F]/g, '');
        if (value) {
            value = '0x' + value.toUpperCase();
        }
        e.target.value = value;
    });

    saveBtn.onclick = async () => {
        const commandName = select.value === 'CUSTOM' ? customInput.value : select.value;
        const code = codeInput.value;

        if (!commandName.trim()) {
            showStatus('Please enter a command name', 'error');
            return;
        }
        if (!code.match(/^0x[0-9A-F]+$/i)) {
            showStatus('Please enter a valid hex code', 'error');
            return;
        }

        try {
            const commands = await (await fetch('/ir/commands')).json();
            commands.buttons[commandName] = {
                protocol: modal.querySelector('#manual-protocol').value,
                code: code,
                bits: parseInt(modal.querySelector('#manual-bits').value),
                description: modal.querySelector('#manual-description').value || commandName
            };

            const saveResponse = await fetch('/ir/commands/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(commands)
            });

            if (saveResponse.ok) {
                showStatus('Command added successfully', 'success');
                loadCommands();
                document.body.removeChild(modal);
            } else {
                throw new Error('Failed to save command');
            }
        } catch (error) {
            showStatus('Error saving command', 'error');
        }
    };

    cancelBtn.onclick = () => {
        document.body.removeChild(modal);
    };

    modal.addEventListener('keyup', (e) => {
        if (e.key === 'Enter') saveBtn.click();
        if (e.key === 'Escape') cancelBtn.click();
    });
}

function showStatus(message, type = '', duration = 3000) {
    const status = document.getElementById('status');
    status.textContent = message;
    status.className = `status ${type} visible`;
    if (duration) {
        setTimeout(() => {
            status.className = 'status';
        }, duration);
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    loadCommands();
});
