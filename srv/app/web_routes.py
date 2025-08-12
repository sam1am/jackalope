import sqlite3
import os
from flask import Flask, render_template, jsonify, request
# NOTE: send_from_directory is no longer needed
from . import config, state_manager, database_handler

# FIX: Point static_folder to the correct absolute path and set the URL path.
app = Flask(__name__,
            static_folder=config.IMGS_PATH,
            static_url_path=f'/{config.IMGS_FOLDER_NAME}',
            template_folder='../templates')

# --- HTML Page Routes ---


@app.route('/')
def index():
    return render_template('index.html')


# FIX: This manual route is no longer necessary, as Flask now handles it automatically.
# @app.route(f'/{config.IMGS_FOLDER_NAME}/<path:filename>')
# def serve_image(filename):
#     return send_from_directory(config.IMGS_PATH, filename)

# --- API Routes ---


@app.route('/api/status')
def api_status():
    return jsonify({
        "status": state_manager.server_state.get("status"),
        "storage_usage": state_manager.server_state.get("storage_usage"),
        "settings_pending": state_manager.pending_config_command is not None
    })


@app.route('/api/captures')
def api_captures():
    try:
        conn = database_handler.get_db_connection()
        conn.row_factory = sqlite3.Row
        captures_from_db = conn.execute(
            "SELECT * FROM captures ORDER BY timestamp DESC").fetchall()
        conn.close()

        captures_list = []
        for row in captures_from_db:
            row_dict = dict(row)
            # This part is still correct: build the full URL path for the frontend.
            row_dict['image_path'] = f"{config.IMGS_FOLDER_NAME}/{os.path.basename(row_dict['image_path'])}"
            captures_list.append(row_dict)

        return jsonify(captures_list)
    except sqlite3.Error as e:
        print(f"Database select error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/settings', methods=['GET'])
def get_settings():
    return jsonify(state_manager.server_state["settings"])


@app.route('/api/settings', methods=['POST'])
def set_settings():
    data = request.get_json()
    if not data:
        return jsonify({"error": "Invalid data"}), 400

    if state_manager.pending_config_command:
        return jsonify({"error": "A previous settings change is still pending. Please wait."}), 429

    try:
        freq = int(data['frequency'])
        thresh = int(data['threshold'])
    except (ValueError, TypeError, KeyError):
        return jsonify({"error": "Invalid or missing frequency/threshold"}), 400

    if not (freq >= 3):
        return jsonify({"error": "Frequency must be 3 seconds or greater."}), 400
    if not (2 <= thresh <= 95):
        return jsonify({"error": "Threshold must be between 2% and 95%."}), 400

    state_manager.server_state["settings"]["frequency"] = freq
    state_manager.server_state["settings"]["threshold"] = thresh
    state_manager.pending_config_command = f"F:{freq},T:{thresh}"
    state_manager.server_state["status"] = "Settings queued. Will send on next connection."

    return jsonify({"message": "Settings queued successfully"})

# --- Flask App Runner ---


def run_flask_app():
    """Starts the Flask web server."""
    print(f"Starting Flask web server on http://0.0.0.0:{config.FLASK_PORT}")
    app.run(host='0.0.0.0', port=config.FLASK_PORT, debug=False)
