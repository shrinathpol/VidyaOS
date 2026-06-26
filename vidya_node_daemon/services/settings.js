const fs = require('fs');
const path = require('path');

const SETTINGS_FILE = path.join(__dirname, '..', 'vidya_settings.json');

// Blueprint for a local secure storage engine managed by Node.js.
// This allows the Tauri/Electron UI to sync settings instantly.
class SettingsService {
    constructor() {
        this.cache = this.loadSettings();
    }

    loadSettings() {
        if (!fs.existsSync(SETTINGS_FILE)) {
            const defaultSettings = {
                theme: 'dark',
                resolution: { width: 1920, height: 1080 },
                googleAuth: {
                    refresh_token: null
                }
            };
            fs.writeFileSync(SETTINGS_FILE, JSON.stringify(defaultSettings, null, 2));
            return defaultSettings;
        }
        return JSON.parse(fs.readFileSync(SETTINGS_FILE, 'utf-8'));
    }

    saveSettings() {
        fs.writeFileSync(SETTINGS_FILE, JSON.stringify(this.cache, null, 2));
    }

    get(key) {
        return this.cache[key];
    }

    set(key, value) {
        this.cache[key] = value;
        this.saveSettings();
    }
}

module.exports = new SettingsService();
