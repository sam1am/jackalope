import asyncio
import datetime
import os
import threading
import functools
import sqlite3
from flask import Flask, render_template, send_from_directory, jsonify
from bleak import BleakScanner, BleakClient
from bleak.exc import BleakError
import time

# --- CONFIGURATION ---
FLASK_PORT = 5550
DEVICE_NAMES = ["T-Camera-BLE-Batch", "T-Camera-BLE"]
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
IMGS_FOLDER_NAME = 'imgs'
IMGS_PATH = os.path.join(ROOT_DIR, IMGS_FOLDER_NAME)
DB_PATH = os.path.join(ROOT_DIR, 'captures.db')

# --- BLE UUIDS ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_ACKNOWLEDGE = b'A'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__, static_folder=IMGS_FOLDER_NAME)
server_state = {"status": "Initializing..."}
data_queue = None
device_found_event = None
found_device = None

# --- DATABASE SETUP ---


def setup_database():
    if not os.path.exists(IMGS_PATH):
        os.makedirs(IMGS_PATH)
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
    conn.close()


def db_insert_capture(timestamp, image_path, gps_lat=None, gps_lon=None):
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute(
        "INSERT INTO captures (timestamp, image_path, gps_lat, gps_lon) VALUES (?, ?, ?, ?)",
        (timestamp, image_path, gps_lat, gps_lon)
    )
    conn.commit()
    conn.close()

# --- BLE LOGIC ---


async def transfer_file_data(client, expected_size, buffer, data_type):
    if expected_size == 0:
        return True
    bytes_received = 0
    try:
        # --- MODIFIED LOGIC ---
        # 1. Wait for the FIRST chunk. The ESP32 sends this automatically after we ACK the file start.
        print(f"Waiting for first chunk of {data_type}...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(
            f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')

        # 2. Now loop for the rest of the data, requesting each subsequent chunk with 'N'.
        while bytes_received < expected_size:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
            print(
                f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')
        # --- END MODIFIED LOGIC ---

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


async def status_notification_handler(sender, data, client):
    try:
        status_str = data.decode('utf-8').strip()
        print(f"\n[STATUS] Received: {status_str}")

        if status_str.startswith("COUNT:"):
            parts = status_str.split(':')
            image_count = int(parts[1])
            server_state["status"] = f"Batch of {image_count} images detected. Acknowledging."
            print(server_state["status"])
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
            print("Sent ACK for batch start")

        elif status_str.startswith("IMAGE:"):
            parts = status_str.split(':')
            img_size = int(parts[1])
            server_state["status"] = f"Receiving image ({img_size} bytes)..."
            print(f"Image transfer starting. Expecting {img_size} bytes.")
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_ACKNOWLEDGE, response=False)
            print("Sent ACK for image start")

            img_buffer = bytearray()
            if await transfer_file_data(client, img_size, img_buffer, "Image"):
                timestamp = datetime.datetime.now()
                filename = timestamp.strftime("%Y-%m-%d_%H-%M-%S-%f") + ".jpg"
                filepath = os.path.join(IMGS_PATH, filename)
                with open(filepath, "wb") as f:
                    f.write(img_buffer)
                db_insert_capture(timestamp.isoformat(),
                                  os.path.join(IMGS_FOLDER_NAME, filename))
                print(f"-> Saved image to {filepath} and logged to DB.")
                server_state["status"] = f"Image saved: {filename}"
            else:
                print("Image transfer failed.")
                server_state["status"] = "Image transfer failed"
    except Exception as e:
        print(f"Error in status_notification_handler: {e}")
        import traceback
        traceback.print_exc()


def detection_callback(device, advertising_data):
    global found_device, device_found_event
    service_uuids = advertising_data.service_uuids or []

    is_target = False
    if device.name:
        for target_name in DEVICE_NAMES:
            if target_name in device.name:
                is_target = True
                break

    if not is_target and SERVICE_UUID.lower() in [s.lower() for s in service_uuids]:
        is_target = True

    if is_target:
        print(f"[SCAN] Target device found: {device.address} ({device.name})")
        if device_found_event and not device_found_event.is_set():
            found_device = device
            device_found_event.set()


async def ble_communication_task():
    global found_device, data_queue, device_found_event
    data_queue = asyncio.Queue()
    device_found_event = asyncio.Event()

    while True:
        server_state["status"] = f"Scanning for camera device..."
        print(f"\n{server_state['status']}")

        scanner = BleakScanner(detection_callback=detection_callback)

        try:
            await scanner.start()
            await asyncio.wait_for(device_found_event.wait(), timeout=120.0)
            await scanner.stop()
        except asyncio.TimeoutError:
            print(f"No device found after 120 seconds. Restarting scan...")
            await scanner.stop()
            await asyncio.sleep(2)
            continue
        except Exception as e:
            print(f"Error during scan: {e}")
            await scanner.stop()
            await asyncio.sleep(5)
            continue

        if found_device:
            server_state["status"] = f"Connecting to {found_device.address}..."
            print(server_state["status"])
            try:
                async with BleakClient(found_device, timeout=20.0) as client:
                    if client.is_connected:
                        server_state["status"] = "Connected. Setting up notifications..."
                        print(server_state["status"])

                        while not data_queue.empty():
                            data_queue.get_nowait()

                        status_handler_with_client = functools.partial(
                            status_notification_handler, client=client)

                        await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                        print("STATUS notifications enabled")
                        await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)
                        print("DATA notifications enabled")

                        server_state["status"] = "Ready. Waiting for device to send data..."
                        print(server_state["status"])

                        while client.is_connected:
                            await asyncio.sleep(1)
                        print("Client disconnected")

            except Exception as e:
                server_state["status"] = f"Connection Error: {e}"
                print(f"An unexpected error occurred: {e}")
            finally:
                server_state["status"] = "Disconnected. Resuming scan."
                print(server_state["status"])
                device_found_event.clear()
                found_device = None
                await asyncio.sleep(2)
        else:
            device_found_event.clear()
            await asyncio.sleep(2)


# --- Flask Web Server ---
@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/status')
def api_status():
    return jsonify(server_state)


@app.route('/api/captures')
def api_captures():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()
    captures = cursor.execute(
        "SELECT * FROM captures ORDER BY timestamp DESC").fetchall()
    conn.close()
    return jsonify([dict(row) for row in captures])


@app.route('/imgs/<path:filename>')
def serve_image(filename):
    return send_from_directory(IMGS_PATH, filename)


def run_flask_app():
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    setup_database()
    flask_thread = threading.Thread(target=run_flask_app, daemon=True)
    flask_thread.start()
    time.sleep(1)

    try:
        print("Starting BLE communication task...")
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    except Exception as e:
        print(f"Fatal error in BLE task: {e}")
        import traceback
        traceback.print_exc()
