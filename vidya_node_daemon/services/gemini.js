const { GoogleGenerativeAI } = require('@google/generative-ai');

// Blueprint for the Gemini AI Service running as a Node.js background worker
class GeminiService {
    constructor() {
        // In a real OS, this API key would be stored securely in an environment variable
        // or fetched after Google Auth. For blueprint purposes, we check process.env.
        this.apiKey = process.env.GEMINI_API_KEY || 'MOCK_API_KEY';
        this.genAI = new GoogleGenerativeAI(this.apiKey);
        this.model = this.genAI.getGenerativeModel({ model: "gemini-1.5-flash" });
    }

    async analyzeTelemetry(telemetryData) {
        if (this.apiKey === 'MOCK_API_KEY') {
            console.log("[Gemini Service] Using Mock Mode (No API key provided).");
            return `Based on telemetry (CPU: ${telemetryData.cpu}%, RAM: ${telemetryData.ram}MB), your system is running smoothly.`;
        }

        const prompt = `You are the VidyaOS Desktop Assistant. Analyze the following hardware telemetry and give a brief 1-sentence insight to the user: ${JSON.stringify(telemetryData)}`;
        
        try {
            const result = await this.model.generateContent(prompt);
            const response = await result.response;
            return response.text();
        } catch (error) {
            console.error("[Gemini Service] Error generating content:", error);
            return "Unable to analyze telemetry at this time.";
        }
    }
}

module.exports = new GeminiService();
