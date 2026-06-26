const resolutionAddon = require('./build/Release/resolution.node');

console.log("Testing Native Addon Bindings...");
try {
    const success = resolutionAddon.setResolution(1920, 1080);
    if (success) {
        console.log("Successfully bridged into C++ and changed resolution.");
    }
} catch (e) {
    console.error("Test failed: ", e.message);
}
