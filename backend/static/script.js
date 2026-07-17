let lastTimestamp = "";
let healthData = {};

setInterval(async () => {
    try {

        const response = await fetch("/api/vitals");
        const data = await response.json();

        if (data.timestamp !== lastTimestamp && data.heart_rate !== "--") {

            lastTimestamp = data.timestamp;
            healthData = data;

            updateDashboard(data);

            generateConsultation();
        }

    } catch (error) {

        console.error("Connection lost:", error);

    }

}, 2000);

function updateDashboard(data) {

    document.getElementById("hr-val").innerText = data.heart_rate;
    document.getElementById("spo2-val").innerText = data.spo2;
    document.getElementById("temp-val").innerText = data.temperature;

    document.getElementById("status-text").innerText =
        "Live Data • " + data.timestamp;

}

async function generateConsultation() {

    if (!API_KEY || API_KEY.includes("PASTE")) {

        addMessage(
            "ai",
            "Please configure your Gemini API Key inside the backend."
        );

        return;
    }

    const prompt = `
I have received the following patient vitals.

Heart Rate: ${healthData.heart_rate} BPM
SpO₂: ${healthData.spo2} %
Temperature: ${healthData.temperature} °C

Provide a short wellness analysis.
Mention whether the values appear normal.
Keep the response concise and include a reminder that this is not a medical diagnosis.
`;

    addMessage("ai", "Analyzing latest vitals...");

    await callGemini(prompt);

}
async function sendMessage() {

    const input = document.getElementById("user-input");
    const message = input.value.trim();

    if (!message) return;

    addMessage("user", message);

    input.value = "";

    const prompt = `
Patient Context

Heart Rate: ${healthData.heart_rate} BPM
SpO₂: ${healthData.spo2} %
Temperature: ${healthData.temperature} °C

User Question:
${message}

Answer the user's question using the above vitals as context.
Keep the answer informative and concise.
Always remind the user that this is not a medical diagnosis.
`;

    await callGemini(prompt);

}

async function callGemini(prompt) {

    if (!API_KEY || API_KEY.includes("PASTE")) {

        addMessage(
            "ai",
            "Gemini API Key has not been configured."
        );

        return;
    }

    const url =
        `https://generativelanguage.googleapis.com/v1beta/models/${MODEL_NAME}:generateContent?key=${API_KEY}`;

    try {

        const response = await fetch(url, {

            method: "POST",

            headers: {
                "Content-Type": "application/json"
            },

            body: JSON.stringify({

                contents: [
                    {
                        parts: [
                            {
                                text: prompt
                            }
                        ]
                    }
                ]

            })

        });

        const data = await response.json();

        if (data.error) {

            addMessage(
                "ai",
                "AI Error: " + data.error.message
            );

            return;
        }

        const reply =
            data.candidates[0].content.parts[0].text;

        addMessage("ai", marked.parse(reply));

    }

    catch (error) {

        console.error(error);

        addMessage(
            "ai",
            "Unable to connect to MedBot AI."
        );

    }

}

function addMessage(sender, message) {

    const chatBox = document.getElementById("chat-box");

    const div = document.createElement("div");

    div.className = `msg ${sender}`;

    div.innerHTML = message;

    chatBox.appendChild(div);

    chatBox.scrollTop = chatBox.scrollHeight;

}

document
    .getElementById("user-input")
    .addEventListener("keypress", function(event) {

        if (event.key === "Enter") {

            sendMessage();

        }

    });