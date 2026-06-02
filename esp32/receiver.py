import socket
import struct

UDP_IP = "0.0.0.0"
UDP_PORT = 8888
FILENAME = "output.qoa"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

total_samples_per_channel = 0
# Calculate frames per network packet: ESP32 SAMPLES_PER_READ / 2
# e.g., if SAMPLES_PER_READ is 240, each packet has 120 sample frames per channel.
SAMPLES_PER_PACKET = 120

print("Recording live QOA audio stream...")

# Open in read/write binary mode so we can seek back to the beginning later
with open(FILENAME, 'w+b') as f:
    # 1. Write dummy header first (with 0 samples)
    f.write(struct.pack('>4sI', b'qoaf', 0))

    try:
        while True:
            data, addr = sock.recvfrom(4096)
            f.write(data)

            # Every network packet contains our fixed frame length
            total_samples_per_channel += SAMPLES_PER_PACKET

    except KeyboardInterrupt:
        print("\nRecording stopped. Finalizing header...")

        # 2. Seek back to byte 4 (right where the uint32_t samples field starts)
        f.seek(4)

        # 3. Overwrite the 0 with the actual computed sample count
        f.write(struct.pack('>I', total_samples_per_channel))

        print(f"File closed cleanly. Total samples per channel: {total_samples_per_channel}")