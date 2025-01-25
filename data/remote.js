import { showStatus } from './utils.js';

async function sendIRCommand(command) {
    try {
        // Загрузить команды
        const response = await fetch('/ir/commands');
        const data = await response.json();

        if (!data.buttons || !data.buttons[command]) {
            throw new Error(`Command "${command}" not found`);
        }

        // Отправить команду
        const result = await fetch('/ir/send', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data.buttons[command])
        });

        if (!result.ok) {
            throw new Error(`HTTP error: ${result.status}`);
        }

        return result;
    } catch (error) {
        console.error('Error sending IR command:', error);
        throw error;
    }
}

class RemoteControl {
    constructor() {
        this.isTransmitting = false;
        this.commandQueue = [];
        this.processingQueue = false;
        this.statusElement = document.getElementById('status');
        this.init();
    }

    init() {
        this.initKeyMap();
        this.initEventListeners();
        this.startQueueProcessor();
    }

    async startQueueProcessor() {
        while (true) {
            if (this.commandQueue.length > 0 && !this.processingQueue) {
                this.processingQueue = true;
                const { command, button } = this.commandQueue.shift();
                await this.sendCommand(command, button);
                this.processingQueue = false;
            }
            await new Promise(resolve => setTimeout(resolve, 100));
        }
    }

    queueCommand(command, button) {
        this.commandQueue.push({ command, button });
    }

    initKeyMap() {
        this.keyMap = {
            // Цифровые кнопки
            ...[...Array(10)].reduce((acc, _, i) => ({ ...acc, [i.toString()]: i.toString() }), {}),
            // Навигация
            'Enter': 'ENTER',
            'Escape': 'BACK',
            'Backspace': 'BACK',
            // Управление каналами и громкостью
            'ArrowUp': 'CH_UP',
            'ArrowDown': 'CH_DOWN',
            'm': 'MUTE',
            'M': 'MUTE',
            'p': 'POWER',
            'P': 'POWER',
            '+': 'VOL_UP',
            '-': 'VOL_DOWN'
        };
    }

    initEventListeners() {
        // Обработчики кнопок
        document.querySelectorAll('.remote-button[data-command]').forEach(button => {
            button.addEventListener('click', (e) => {
                e.preventDefault();
                this.queueCommand(button.dataset.command, button);
            });
        });

        // Обработчики клавиатуры
        document.addEventListener('keydown', (e) => this.handleKeyPress(e));

        // Предотвращение зума на мобильных устройствах
        this.initMobileHandlers();
    }

    initMobileHandlers() {
        document.addEventListener('touchmove', (e) => {
            if (e.touches.length > 1) e.preventDefault();
        }, { passive: false });
        document.addEventListener('gesturestart', (e) => e.preventDefault());
    }

    async sendCommand(command, button) {
        if (this.isTransmitting) return;

        const originalBackground = button.style.background;
        this.isTransmitting = true;
        button.style.background = 'var(--button-active)';

        try {
            showStatus(`Sending: ${command}`);
            const response = await sendIRCommand(command);

            if (!response.ok) {
                throw new Error(`Failed to send command: ${response.status}`);
            }

            showStatus(`Sent: ${command}`, 'success');

            // Добавляем вибрацию, если устройство поддерживает
            if ('vibrate' in navigator) {
                navigator.vibrate(50);
            }
        } catch (error) {

            console.error('Send error:', error);
            showStatus(error.message, 'error');
        } finally {
            this.isTransmitting = false;
            button.style.background = originalBackground;
        }
    }

    handleKeyPress(e) {
        const command = this.keyMap[e.key];
        if (command) {
            const button = document.querySelector(`[data-command="${command}"]`);
            if (button) {
                this.queueCommand(command, button);
            }
        }
    }
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    new RemoteControl();
});
