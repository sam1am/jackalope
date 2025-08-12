import asyncio
import datetime
import os
import threading
import functools
import sqlite3
import time
from flask import Flask, render_template, send_from_directory, jsonify, request
from bleak import BleakScanner, BleakClient

# --- CONFIGURATION ---
FLASK_PORT = 5550
DEVICE_NAMES = ["T-Camera-BLE-Batch", "T-Camera-BLE"]
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
IMGS_FOLDER_NAME = 'imgs'
IMGS_PATH = os.path.join(ROOT_DIR, IMGS_FOLDER_NAME)
DB_PATH = os.path.join(ROOT_DIR, 'captures.db')

# --- BLE UUIDs ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_CONFIG = "a31a6820-8437-4f55-8898-5226c04a29a3"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_ACKNOWLEDGE = b'A'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__, static_folder=IMGS_FOLDER_NAME)
server_state = {
    "status": "Initializing...",
    "storage_usage": 0,
    "settings": {"frequency": 30, "threshold": 80}
}
data_queue = None
device_found_event = None
found_device = None
pending_config_command = None


# --- DATABASE HELPERS ---
def get_db_connection():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS captures (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            image_path TEXT NOT NULL,
            gps_lat REAL,
            gps_lon REAL
        )
    ''')
    conn.commit()
    return conn


def setup_filesystem():
    if not os.path.exists(IMGS_PATH):
        print(f"Image directory not found. Creating at: {IMGS_PATH}")
        os.makedirs(IMGS_PATH)
    conn = get_db_connection()
    conn.close()
    print("Filesystem and database are ready.")


def db_insert_capture(timestamp, image_path):
    try:
        conn = get_db_connection()
        cursor = conn.cursor()
        cursor.execute(
            "INSERT INTO captures (timestamp, image_path) VALUES (?, ?)", (timestamp, image_path))
        conn.commit()
        conn.close()
    except sqlite3.Error as e:
        print(f"Database insert error: {e}")


# --- BLE LOGIC ---
async def transfer_file_data(client, expected_size, buffer, data_type):
    if expected_size == 0:
        return True
    bytes_received = 0
    try:
        print(f"Waiting for first chunk of {data_type}...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(
            f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
        while bytes_received < expected_size:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
            print(
                f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
    except asyncio.TimeoutError:
        print(
            f"\nERROR: Timeout waiting for {data_type} data at {bytes_received}/{expected_size} bytes.")
        return False
    print(
        f"\n-> {data_type} transfer complete ({bytes_received} bytes received).")
    return True


def data_notification_handler(sender, data):
    if data_queue:
        data_queue.put_nowait(data)


async def handle_image_transfer(client, img_size):
    """
    Handles the image transfer in a separate, robust task to avoid blocking the main event handler.
    """
    try:
        server_state["status"] = f"Receiving image ({img_size} bytes)..."
        # Acknowledge the image transfer start
        await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
        img_buffer = bytearray()
        if await transfer_file_data(client, img_size, img_buffer, "Image"):
            timestamp = datetime.datetime.now()
            filename = timestamp.strftime("%Y-%m-%d_%H-%M-%S-%f") + ".jpg"
            filepath = os.path.join(IMGS_PATH, filename)
            with open(filepath, "wb") as f:
                f.write(img_buffer)
            db_insert_capture(timestamp.isoformat(),
                              os.path.join(IMGS_FOLDER_NAME, filename))
            print(f"-> Saved image to {filepath}")
            server_state["status"] = f"Image saved: {filename}"
        else:
            server_state["status"] = "Image transfer failed"
    except Exception as e:
        print(f"\nError during image transfer task: {e}")
        server_state["status"] = "Image transfer failed due to connection error."


async def status_notification_handler(sender, data, client):
    """
    Handles incoming status notifications from the device.
    For long operations like image transfers, it spawns a new task.
    """
    try:
        status_str = data.decode('utf-8').strip()
        print(f"\n[STATUS] Received: {status_str}")

        if status_str.startswith("PSRAM:"):
            try:
                usage_val = float(status_str.split(':')[1].split('%')[0])
                server_state["storage_usage"] = round(usage_val, 1)
            except (ValueError, IndexError):
                pass
            server_state["status"] = "Device ready to transfer."

        elif status_str.startswith("COUNT:"):
            image_count = int(status_str.split(':')[1])
            server_state["status"] = f"Batch of {image_count} images incoming. Acknowledging."
            print(server_state["status"])
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)

        elif status_str.startswith("IMAGE:"):
            img_size = int(status_str.split(':')[1])
            # FIX: Spawn the transfer as a background task so it doesn't block this handler
            asyncio.create_task(handle_image_transfer(client, img_size))

    except Exception as e:
        print(f"Error in status_notification_handler: {e}")


def detection_callback(device, advertising_data):
    global found_device, device_found_event
    if device.name and any(name in device.name for name in DEVICE_NAMES):
        print(f"[SCAN] Target device found: {device.address} ({device.name})")
        if device_found_event and not device_found_event.is_set():
            found_device = device
            device_found_event.set()


async def ble_communication_task():
    global found_device, data_queue, device_found_event, pending_config_command
    data_queue = asyncio.Queue()
    device_found_event = asyncio.Event()

    while True:
        server_state["status"] = "Scanning for camera device..."
        print(f"\n{server_state['status']}")
        scanner = BleakScanner(detection_callback=detection_callback)
        try:
            await scanner.start()
            await asyncio.wait_for(device_found_event.wait(), timeout=120.0)
            await scanner.stop()
        except asyncio.TimeoutError:
            await scanner.stop()
            continue
        except Exception as e:
            print(f"Error during scan: {e}")
            await scanner.stop()
            continue

        if found_device:
            server_state["status"] = f"Connecting to {found_device.address}..."
            try:
                async with BleakClient(found_device, timeout=20.0) as client:
                    if client.is_connected:
                        server_state["status"] = "Connected. Setting up notifications..."
                        while not data_queue.empty():
                            data_queue.get_nowait()

                        status_handler_with_client = functools.partial(
                            status_notification_handler, client=client)
                        await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                        await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)

                        server_state["status"] = "Ready. Waiting for device data..."
                        while client.is_connected:
                            if pending_config_command:
                                print(
                                    f"Sending config command: {pending_config_command}")
                                try:
                                    await client.write_gatt_char(
                                        CHARACTERISTIC_UUID_CONFIG,
                                        bytearray(
                                            pending_config_command, 'utf-8'),
                                        response=False
                                    )
                                    pending_config_command = None  # Clear after sending
                                    server_state["status"] = "Settings sent to device."
                                except Exception as e:
                                    print(f"Failed to send config: {e}")
                            await asyncio.sleep(1)
                        print("Client disconnected.")
            except Exception as e:
                server_state["status"] = f"Connection Error: {e}"
            finally:
                server_state["status"] = "Disconnected. Resuming scan."
                device_found_event.clear()
                found_device = None
                await asyncio.sleep(2)


# --- Flask Web Server ---
@app.route('/')
def index(): return render_template('index.html')


@app.route('/api/status')
def api_status():
    return jsonify({"status": server_state.get("status"), "storage_usage": server_state.get("storage_usage")})


@app.route('/api/captures')
def api_captures():
    try:
        conn = get_db_connection()
        conn.row_factory = sqlite3.Row
        captures = conn.execute(
            "SELECT * FROM captures ORDER BY timestamp DESC").fetchall()
        conn.close()
        return jsonify([dict(row) for row in captures])
    except sqlite3.Error as e:
        print(f"Database select error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route('/api/settings', methods=['GET'])
def get_settings(): return jsonify(server_state["settings"])


@app.route('/api/settings', methods=['POST'])
def set_settings():
    global pending_config_command
    data = request.get_json()
    if not data:
        return jsonify({"error": "Invalid data"}), 400

    try:
        freq = int(data['frequency'])
        thresh = int(data['threshold'])
    except (ValueError, TypeError, KeyError):
        return jsonify({"error": "Invalid or missing frequency/threshold"}), 400

    # FIX: Added server-side validation for the inputs
    if not (freq >= 5):
        return jsonify({"error": "Frequency must be 5 seconds or greater."}), 400
    if not (10 <= thresh <= 95):
        return jsonify({"error": "Threshold must be between 10% and 95%."}), 400

    server_state["settings"]["frequency"] = freq
    server_state["settings"]["threshold"] = thresh
    pending_config_command = f"F:{freq},T:{thresh}"

    server_state["status"] = "Settings queued. Will send on next connection."
    return jsonify({"message": "Settings queued successfully"})


@app.route('/imgs/<path:filename>')
def serve_image(filename): return send_from_directory(IMGS_PATH, filename)


def run_flask_app():
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    setup_filesystem()
    threading.Thread(target=run_flask_app, daemon=True).start()
    time.sleep(1)
    try:
        print("Starting BLE communication task...")
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    except Exception as e:
        print(f"Fatal error in BLE task: {e}")
