import socket
import wave

UDP_IP = "0.0.0.0"
UDP_PORT = 8888
FILENAME = "output.wav"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

with wave.open(FILENAME, 'wb') as wav:
    wav.setnchannels(2)      # 2 channels (Left and Right)
    wav.setsampwidth(3)      # 24-bit (3 bytes per sample)
    wav.setframerate(22050)  # sample rate

    print("Recording tightly packed 24-bit audio stream...")
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            wav.writeframes(data)
    except KeyboardInterrupt:
        print("\nRecording stopped.")