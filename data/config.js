async function loadConfig(forceReload = false) {
    try {
        const response = await fetch("/config.json?" + (forceReload ? Date.now() : ""));
        const config = await response.json();

        for (const section of ["wifi", "ntp", "mqtt"]) {
            if (!config[section]) continue;
            document.querySelectorAll(`[name^="${section}."]`).forEach((input) => {
                const field = input.name.split(".")[1];
                if (config[section].hasOwnProperty(field)) {
                    input.value = config[section][field];
                }
            });
        }
    } catch (error) {
        showStatus("Failed to load configuration", "error");
    }
}

function showStatus(message, type = "", duration = 3000) {
    const status = document.getElementById("status");
    status.className = type ? `status ${type}` : "status";
    status.textContent = message;
    if (duration) setTimeout(() => {
        status.className = "";
        status.textContent = "";
    }, duration);
}

// Остальные функции config.js...
