from flask import Flask, request, jsonify, render_template
import datetime

app = Flask(__name__)

# ==========================================================
# CONFIGURATION
# ==========================================================

GEMINI_API_KEY = ""      # Add your Gemini API Key
GEMINI_MODEL_ID = ""     # Example: gemini-2.5-flash

# ==========================================================
# GLOBAL VARIABLES
# ==========================================================

latest_vitals = {
    "heart_rate": "--",
    "spo2": "--",
    "temperature": "--",
    "timestamp": "Waiting for data..."
}

# ==========================================================
# ROUTES
# ==========================================================

@app.route("/")
def home():
    return render_template(
        "index.html",
        api_key=GEMINI_API_KEY,
        model_name=GEMINI_MODEL_ID
    )


@app.route("/data", methods=["POST"])
def receive_data():

    global latest_vitals

    data = request.json

    latest_vitals = {
        "heart_rate": data.get("heart_rate"),
        "spo2": data.get("spo2"),
        "temperature": data.get("temperature"),
        "timestamp": datetime.datetime.now().strftime("%H:%M:%S")
    }

    print(
        f"[{latest_vitals['timestamp']}] "
        f"HR={latest_vitals['heart_rate']}  "
        f"SpO₂={latest_vitals['spo2']}  "
        f"Temp={latest_vitals['temperature']}"
    )

    return jsonify({"status": "success"}), 200


@app.route("/api/vitals")
def get_vitals():
    return jsonify(latest_vitals)


# ==========================================================
# MAIN
# ==========================================================

if __name__ == "__main__":
    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True
    )