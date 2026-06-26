const settings = require('./services/settings');
const gemini = require('./services/gemini');
const fs = require('fs');
const path = require('path');
const os = require('os');

// Attempt to load the C++ Native Addon
let resolutionAddon;
try {
    resolutionAddon = require('./build/Release/resolution.node');
    console.log("[VidyaOS Node Daemon] Successfully loaded C++ Resolution Addon via N-API.");
} catch (e) {
    console.warn("[VidyaOS Node Daemon] Warning: Native addon not compiled yet. Run `node-gyp rebuild`.");
}

console.log("\n=== VidyaOS Node.js Orchestrator Starting ===");

// 1. Read Current Settings
const currentRes = settings.get('resolution');
console.log(`[Daemon] Loaded Settings - Resolution: ${currentRes.width}x${currentRes.height}`);

// 2. Trigger C++ Hardware hook natively
if (resolutionAddon) {
    console.log("[Daemon] Invoking native C++ resolution change...");
    resolutionAddon.setResolution(currentRes.width, currentRes.height);
}

// 3. Setup Gemini Asynchronous polling blueprint
async function runGeminiCheck() {
    console.log("\n[Daemon] Requesting Gemini AI Analysis...");
    // Mocking telemetry data that would normally be read from the C++ backend
    const mockTelemetry = { cpu: 45, ram: 1024, temp: 40 };
    
    const insight = await gemini.analyzeTelemetry(mockTelemetry);
    console.log(`[Gemini UI Overlay] Assistant says: "${insight}"`);
}

// Run the check after a brief delay
setTimeout(runGeminiCheck, 1000);

// --- Cloud Sync Polling Mechanism ---

function getSandboxRoot() {
    if (process.env.VIDYAOS_SANDBOX_ROOT) {
        return process.env.VIDYAOS_SANDBOX_ROOT;
    }
    const home = os.homedir();
    if (home) {
        const configPath = path.join(home, '.vidyaos', 'config');
        if (fs.existsSync(configPath)) {
            try {
                const content = fs.readFileSync(configPath, 'utf8').trim();
                if (content) return content;
            } catch (err) {
                // ignore
            }
        }
        return path.join(home, '.vidyaos', 'rootfs');
    }
    return path.resolve('./.vidyaos/rootfs');
}

const sandboxRoot = getSandboxRoot();
const cloudDir = path.join(os.homedir(), '.vidyaos', 'cloud');

const cmdFile = path.join(sandboxRoot, 'var', 'run', 'cloud_cmd.json');
const statusFile = path.join(sandboxRoot, 'var', 'run', 'cloud_status.json');

console.log(`[Cloud Sync] Watching for commands at: ${cmdFile}`);
console.log(`[Cloud Sync] Cloud directory target: ${cloudDir}`);

function pollCloudCommands() {
    if (fs.existsSync(cmdFile)) {
        console.log(`[Cloud Sync] Found command file: ${cmdFile}`);
        let cmdData;
        try {
            const content = fs.readFileSync(cmdFile, 'utf8');
            cmdData = JSON.parse(content);
        } catch (err) {
            console.error(`[Cloud Sync] Error reading/parsing command file: ${err.message}`);
        }

        // Delete command file to acknowledge receipt
        try {
            fs.unlinkSync(cmdFile);
        } catch (err) {
            console.error(`[Cloud Sync] Error deleting command file: ${err.message}`);
        }

        if (cmdData && cmdData.command) {
            const cmd = cmdData.command;
            console.log(`[Cloud Sync] Executing command: ${cmd}`);

            if (cmd === 'sync') {
                try {
                    if (!fs.existsSync(cloudDir)) {
                        fs.mkdirSync(cloudDir, { recursive: true });
                    }
                    
                    const srcSettings = path.join(sandboxRoot, 'etc', 'vidya', 'settings.json');
                    const destSettings = path.join(cloudDir, 'settings.json');
                    if (fs.existsSync(srcSettings)) {
                        fs.copyFileSync(srcSettings, destSettings);
                        console.log(`[Cloud Sync] Synced settings.json to cloud.`);
                    }

                    const srcFootprint = path.join(sandboxRoot, 'var', 'footprint.db');
                    const destFootprint = path.join(cloudDir, 'footprint.db');
                    if (fs.existsSync(srcFootprint)) {
                        fs.copyFileSync(srcFootprint, destFootprint);
                        console.log(`[Cloud Sync] Synced footprint.db to cloud.`);
                    }

                    // Create status dir if not exists
                    const statusDir = path.dirname(statusFile);
                    if (!fs.existsSync(statusDir)) {
                        fs.mkdirSync(statusDir, { recursive: true });
                    }

                    fs.writeFileSync(statusFile, JSON.stringify({ status: "success", operation: "sync" }));
                    console.log(`[Cloud Sync] Sync operation completed successfully.`);
                } catch (err) {
                    console.error(`[Cloud Sync] Sync failed: ${err.message}`);
                    try {
                        fs.writeFileSync(statusFile, JSON.stringify({ status: "failed", operation: "sync", error: err.message }));
                    } catch (e) {}
                }
            } else if (cmd === 'restore') {
                try {
                    const srcSettings = path.join(cloudDir, 'settings.json');
                    const destSettings = path.join(sandboxRoot, 'etc', 'vidya', 'settings.json');
                    const srcFootprint = path.join(cloudDir, 'footprint.db');
                    const destFootprint = path.join(sandboxRoot, 'var', 'footprint.db');

                    let settingsRestored = false;
                    let footprintRestored = false;

                    // Ensure target directories exist inside sandbox
                    const destSettingsDir = path.dirname(destSettings);
                    if (!fs.existsSync(destSettingsDir)) {
                        fs.mkdirSync(destSettingsDir, { recursive: true });
                    }
                    const destFootprintDir = path.dirname(destFootprint);
                    if (!fs.existsSync(destFootprintDir)) {
                        fs.mkdirSync(destFootprintDir, { recursive: true });
                    }

                    if (fs.existsSync(srcSettings)) {
                        fs.copyFileSync(srcSettings, destSettings);
                        console.log(`[Cloud Sync] Restored settings.json from cloud.`);
                        settingsRestored = true;
                    }
                    if (fs.existsSync(srcFootprint)) {
                        fs.copyFileSync(srcFootprint, destFootprint);
                        console.log(`[Cloud Sync] Restored footprint.db from cloud.`);
                        footprintRestored = true;
                    }

                    // Create status dir if not exists
                    const statusDir = path.dirname(statusFile);
                    if (!fs.existsSync(statusDir)) {
                        fs.mkdirSync(statusDir, { recursive: true });
                    }

                    if (settingsRestored || footprintRestored) {
                        fs.writeFileSync(statusFile, JSON.stringify({ status: "success", operation: "restore" }));
                        console.log(`[Cloud Sync] Restore operation completed successfully.`);
                    } else {
                        fs.writeFileSync(statusFile, JSON.stringify({ status: "failed", operation: "restore", error: "No cloud backups found" }));
                        console.log(`[Cloud Sync] Restore operation failed: No cloud backups found.`);
                    }
                } catch (err) {
                    console.error(`[Cloud Sync] Restore failed: ${err.message}`);
                    try {
                        fs.writeFileSync(statusFile, JSON.stringify({ status: "failed", operation: "restore", error: err.message }));
                    } catch (e) {}
                }
            } else {
                console.warn(`[Cloud Sync] Unknown command: ${cmd}`);
            }
        }
    }
}

// Poll command file every 500ms
setInterval(pollCloudCommands, 500);
