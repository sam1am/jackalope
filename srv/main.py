import asyncio
import datetime
import os
import threading
import functools
from flask import Flask, render_template, send_from_directory
from bleak import BleakScanner, BleakClient

# --- CONFIGURATION ---
RECONNECT_DELAY_SECONDS = 5
CAPTURE_INTERVAL_SECONDS = 2  # How often to request a capture
FLASK_PORT = 5550
DEVICE_NAME = "T-Camera-BLE"

# --- BLE UUIDS ---
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHARACTERISTIC_UUID_DATA = "7347e350-5552-4822-8243-b8923a4114d2"
CHARACTERISTIC_UUID_COMMAND = "a244c201-1fb5-459e-8fcc-c5c9c331914b"

# --- PROTOCOL COMMANDS ---
CMD_NEXT_CHUNK = b'N'
CMD_TRANSFER_COMPLETE = b'C'
CMD_TRIGGER_CAPTURE = b'T'

# --- WEB SERVER & GLOBAL STATE ---
app = Flask(__name__)
STATIC_FOLDER = 'static'
LATEST_IMAGE_FILENAME = 'latest.jpg'
LATEST_AUDIO_FILENAME = 'latest.wav'
LATEST_IMAGE_PATH = os.path.join(STATIC_FOLDER, LATEST_IMAGE_FILENAME)
LATEST_AUDIO_PATH = os.path.join(STATIC_FOLDER, LATEST_AUDIO_FILENAME)
server_state = {"status": "Initializing..."}
transfer_in_progress = asyncio.Event()
data_queue = asyncio.Queue()

# Ensure static folder exists
if not os.path.exists(STATIC_FOLDER):
    os.makedirs(STATIC_FOLDER)


async def transfer_data(client, expected_size, buffer, data_type):
    """
    Receives data from the ESP32. It waits for the first chunk to be "pushed"
    by the device, then "pulls" the remaining chunks.
    """
    if expected_size == 0:
        return True

    print(f"Starting {data_type} transfer. Expecting {expected_size} bytes.")
    bytes_received = 0

    # --- FIX: Wait for the first chunk to be "pushed" by the ESP32 ---
    try:
        print("Waiting for the first pushed chunk...")
        first_chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        data_queue.task_done()
        print(f"Received first pushed chunk ({len(first_chunk)} bytes).")
    except asyncio.TimeoutError:
        print(
            f"ERROR: Timeout waiting for the first pushed {data_type} data chunk.")
        return False
    except Exception as e:
        print(f"ERROR receiving first chunk: {e}")
        return False
    # --- END FIX ---

    # Now, pull the rest of the data
    while bytes_received < expected_size:
        try:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            data_queue.task_done()
        except asyncio.TimeoutError:
            print(
                f"ERROR: Timeout waiting for {data_type} data chunk at {bytes_received}/{expected_size} bytes.")
            return False
        except Exception as e:
            print(f"ERROR in transfer_data loop: {e}")
            return False

    print(f"-> {data_type} transfer complete ({bytes_received} bytes received).")
    return True


def data_notification_handler(sender, data):
    """Callback that puts received data chunks into the async queue."""
    data_queue.put_nowait(data)


async def status_notification_handler(sender, data, client):
    """
    Callback that handles the initial status update, then starts the
    data transfer process for image and audio.
    """
    try:
        status_str = data.decode('utf-8')
        img_size_str, audio_size_str = status_str.split(':')
        img_size = int(img_size_str)
        audio_size = int(audio_size_str)

        print(
            f"\n[STATUS] Received: Image: {img_size} bytes, Audio: {audio_size} bytes.")
        server_state["status"] = f"Receiving: Img({img_size}B) Aud({audio_size}B)"

        # Transfer Image data
        img_buffer = bytearray()
        if not await transfer_data(client, img_size, img_buffer, "Image"):
            print("Image transfer failed.")
        elif img_size > 0:
            with open(LATEST_IMAGE_PATH, "wb") as f:
                f.write(img_buffer)
            print(f"-> Saved latest image to {LATEST_IMAGE_PATH}")

        # Transfer Audio data
        audio_buffer = bytearray()
        if not await transfer_data(client, audio_size, audio_buffer, "Audio"):
            print("Audio transfer failed.")
        elif audio_size > 0:
            with open(LATEST_AUDIO_PATH, "wb") as f:
                f.write(audio_buffer)
            print(f"-> Saved latest audio to {LATEST_AUDIO_PATH}")

    except Exception as e:
        print(f"Error in status_notification_handler: {e}")
    finally:
        # This is CRITICAL: Always acknowledge completion to unblock the ESP32.
        print("--> Sending 'Complete' acknowledgement to ESP32.")
        try:
            await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_TRANSFER_COMPLETE, response=False)
        except Exception as e:
            print(f"Warning: Could not send 'Complete' ack: {e}")

        print("\n--- Cycle Complete ---")
        # Signal to the main loop that this cycle is done.
        transfer_in_progress.set()


async def ble_communication_task():
    """The main async task that connects to the ESP32 and drives the capture loop."""
    while True:
        try:
            server_state["status"] = f"Scanning for {DEVICE_NAME}..."
            print(server_state["status"])
            device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)

            if not device:
                await asyncio.sleep(RECONNECT_DELAY_SECONDS)
                continue

            async with BleakClient(device, timeout=20.0) as client:
                if client.is_connected:
                    print(f"Successfully connected to {device.address}")
                    server_state["status"] = "Connected"

                    # Register notification handlers
                    status_handler_with_client = functools.partial(
                        status_notification_handler, client=client)
                    await client.start_notify(CHARACTERISTIC_UUID_STATUS, status_handler_with_client)
                    await client.start_notify(CHARACTERISTIC_UUID_DATA, data_notification_handler)

                    # Main server-driven capture loop
                    while client.is_connected:
                        print(f"Triggering new capture...")
                        server_state["status"] = "Triggering Capture..."
                        transfer_in_progress.clear()

                        await client.write_gatt_char(CHARACTERISTIC_UUID_COMMAND, CMD_TRIGGER_CAPTURE, response=True)

                        # Wait for the status_handler to signal completion via the event
                        await asyncio.wait_for(transfer_in_progress.wait(), timeout=60.0)

                        server_state["status"] = "Idle, waiting for next cycle..."
                        await asyncio.sleep(CAPTURE_INTERVAL_SECONDS)

        except asyncio.TimeoutError:
            print("Timeout during capture cycle. Retrying...")
            server_state["status"] = "Timeout. Retrying..."
        except Exception as e:
            print(f"An error occurred in ble_communication_task: {e}")
            server_state["status"] = "Connection Lost. Retrying..."
        finally:
            print(
                f"Disconnected. Retrying in {RECONNECT_DELAY_SECONDS} seconds...")
            await asyncio.sleep(RECONNECT_DELAY_SECONDS)

# --- Flask Web Server Setup ---


@app.route('/')
def index():
    """Serves the main HTML page."""
    return render_template('index.html', status=server_state.get("status"))


@app.route(f'/{LATEST_IMAGE_FILENAME}')
def latest_image():
    """Serves the latest captured image."""
    return send_from_directory(STATIC_FOLDER, LATEST_IMAGE_FILENAME, as_attachment=False, mimetype='image/jpeg')


def run_flask_app():
    """Runs the Flask app in a separate thread."""
    print(f"Starting Flask web server on http://0.0.0.0:{FLASK_PORT}")
    app.run(host='0.0.0.0', port=FLASK_PORT, debug=False)


if __name__ == '__main__':
    # Start the Flask server in a background thread
    flask_thread = threading.Thread(target=run_flask_app, daemon=True)
    flask_thread.start()

    # Start the main BLE communication loop
    print("Starting BLE communication task...")
    try:
        asyncio.run(ble_communication_task())
    except KeyboardInterrupt:
        print("Program stopped by user.")
