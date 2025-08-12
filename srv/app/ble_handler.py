import asyncio
import datetime
import os
import functools
from bleak import BleakScanner, BleakClient

from . import config, state_manager, database_handler

# --- BLE DATA TRANSFER ---


async def transfer_file_data(client, expected_size, buffer, data_type):
    """Handles the chunk-by-chunk reception of file data."""
    if expected_size == 0:
        return True
    bytes_received = 0
    try:
        print(f"Waiting for first chunk of {data_type}...")
        first_chunk = await asyncio.wait_for(state_manager.data_queue.get(), timeout=15.0)
        buffer.extend(first_chunk)
        bytes_received = len(buffer)
        state_manager.data_queue.task_done()
        print(
            f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')

        while bytes_received < expected_size:
            await client.write_gatt_char(config.CHARACTERISTIC_UUID_COMMAND, config.CMD_NEXT_CHUNK, response=False)
            chunk = await asyncio.wait_for(state_manager.data_queue.get(), timeout=15.0)
            buffer.extend(chunk)
            bytes_received = len(buffer)
            state_manager.data_queue.task_done()
            print(
                f"Receiving {data_type}: {bytes_received}/{expected_size} bytes", end='\r')

    except asyncio.TimeoutError:
        print(
            f"\nERROR: Timeout waiting for {data_type} data at {bytes_received}/{expected_size} bytes.")
        return False

    print(
        f"\n-> {data_type} transfer complete ({bytes_received} bytes received).")
    return True


async def handle_image_transfer(client, img_size):
    """Manages the complete image transfer process."""
    try:
        state_manager.server_state["status"] = f"Receiving image ({img_size} bytes)..."
        await client.write_gatt_char(config.CHARACTERISTIC_UUID_COMMAND, config.CMD_ACKNOWLEDGE, response=False)
        img_buffer = bytearray()

        if await transfer_file_data(client, img_size, img_buffer, "Image"):
            timestamp = datetime.datetime.now()
            filename = timestamp.strftime("%Y-%m-%d_%H-%M-%S-%f") + ".jpg"
            filepath = os.path.join(config.IMGS_PATH, filename)
            with open(filepath, "wb") as f:
                f.write(img_buffer)

            db_path = os.path.join(config.IMGS_FOLDER_NAME, filename)
            database_handler.db_insert_capture(timestamp.isoformat(), db_path)

            print(f"-> Saved image to {filepath}")
            state_manager.server_state["status"] = f"Image saved: {filename}"
        else:
            state_manager.server_state["status"] = "Image transfer failed"

    except Exception as e:
        print(f"\nError during image transfer task: {e}")
        state_manager.server_state["status"] = "Image transfer failed due to connection error."

# --- BLE NOTIFICATION HANDLERS ---


def data_notification_handler(sender, data):
    """Puts incoming data chunks into the queue."""
    if state_manager.data_queue:
        state_manager.data_queue.put_nowait(data)


def status_notification_handler(sender, data, client, loop):
    """Handles status updates from the BLE device."""
    async def process_status_update():
        try:
            status_str = data.decode('utf-8').strip()
            print(f"\n[STATUS] Received: {status_str}")

            if status_str.startswith("PSRAM:"):
                try:
                    usage_val = float(status_str.split(':')[1].split('%')[0])
                    state_manager.server_state["storage_usage"] = round(
                        usage_val, 1)
                except (ValueError, IndexError):
                    pass
                if not state_manager.pending_config_command:
                    state_manager.server_state["status"] = "Device ready to transfer."

            elif status_str.startswith("COUNT:"):
                image_count = int(status_str.split(':')[1])
                status_msg = f"Batch of {image_count} images incoming. Acknowledging."
                state_manager.server_state["status"] = status_msg
                print(status_msg)
                await client.write_gatt_char(config.CHARACTERISTIC_UUID_COMMAND, config.CMD_ACKNOWLEDGE, response=False)

            elif status_str.startswith("IMAGE:"):
                img_size = int(status_str.split(':')[1])
                asyncio.create_task(handle_image_transfer(client, img_size))

        except Exception as e:
            print(f"Error in process_status_update: {e}")

    asyncio.run_coroutine_threadsafe(process_status_update(), loop)


def detection_callback(device, advertising_data):
    """Callback triggered when a BLE device is found."""
    if device.name and any(name in device.name for name in config.DEVICE_NAMES):
        print(f"[SCAN] Target device found: {device.address} ({device.name})")
        if state_manager.device_found_event and not state_manager.device_found_event.is_set():
            state_manager.found_device = device
            state_manager.device_found_event.set()

# --- MAIN BLE TASK ---


async def ble_communication_task():
    """The main asynchronous task that handles all BLE communication."""
    state_manager.data_queue = asyncio.Queue()
    state_manager.device_found_event = asyncio.Event()
    loop = asyncio.get_running_loop()

    while True:
        if not state_manager.pending_config_command:
            state_manager.server_state["status"] = "Scanning for camera device..."
        print(f"\n{state_manager.server_state['status']}")

        scanner = BleakScanner(detection_callback=detection_callback)
        try:
            await scanner.start()
            await asyncio.wait_for(state_manager.device_found_event.wait(), timeout=120.0)
            await scanner.stop()
        except asyncio.TimeoutError:
            await scanner.stop()
            continue
        except Exception as e:
            print(f"Error during scan: {e}")
            await scanner.stop()
            continue

        if state_manager.found_device:
            if not state_manager.pending_config_command:
                state_manager.server_state[
                    "status"] = f"Connecting to {state_manager.found_device.address}..."
            try:
                async with BleakClient(state_manager.found_device, timeout=20.0) as client:
                    if client.is_connected:
                        state_manager.server_state["status"] = "Connected. Setting up notifications..."
                        while not state_manager.data_queue.empty():
                            state_manager.data_queue.get_nowait()

                        status_handler = functools.partial(
                            status_notification_handler, client=client, loop=loop)
                        await client.start_notify(config.CHARACTERISTIC_UUID_STATUS, status_handler)
                        await client.start_notify(config.CHARACTERISTIC_UUID_DATA, data_notification_handler)

                        print(
                            "Subscribed to notifications. Signaling device that we are ready.")
                        await client.write_gatt_char(config.CHARACTERISTIC_UUID_COMMAND, config.CMD_READY, response=False)

                        if not state_manager.pending_config_command:
                            state_manager.server_state["status"] = "Ready. Waiting for device data..."

                        while client.is_connected:
                            if state_manager.pending_config_command:
                                cmd = state_manager.pending_config_command
                                print(f"Sending config command: {cmd}")
                                try:
                                    await client.write_gatt_char(config.CHARACTERISTIC_UUID_CONFIG, bytearray(cmd, 'utf-8'), response=False)
                                    state_manager.pending_config_command = None
                                    state_manager.server_state["status"] = "Settings sent to device."
                                except Exception as e:
                                    print(f"Failed to send config: {e}")
                            await asyncio.sleep(1)
                        print("Client disconnected.")

            except Exception as e:
                state_manager.server_state["status"] = f"Connection Error: {e}"
            finally:
                if not state_manager.pending_config_command:
                    state_manager.server_state["status"] = "Disconnected. Resuming scan."
                state_manager.device_found_event.clear()
                state_manager.found_device = None
                await asyncio.sleep(2)
