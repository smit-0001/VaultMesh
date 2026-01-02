import socket
import struct
import os

# Configuration
HOST = '127.0.0.1'
PORT = 9000
FILE_TO_SEND = "test_upload.txt"

# Protocol Constants
MAGIC = 0x56       # 'V'
CMD_UPLOAD = 0x01

def create_test_file():
    """Creates a dummy file to upload."""
    with open(FILE_TO_SEND, "w") as f:
        f.write("Hello, VaultMesh! This is a test file from the Python Client.")
    print(f"Created {FILE_TO_SEND}")

def send_file():
    # 1. Prepare Data
    filename = os.path.basename(FILE_TO_SEND)
    filename_bytes = filename.encode('utf-8')
    filesize = os.path.getsize(FILE_TO_SEND)
    
    # 2. Open Socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        try:
            sock.connect((HOST, PORT))
            print(f"Connected to {HOST}:{PORT}")

            # 3. Construct Header
            # Struct Format:
            # B = unsigned char (1 byte) -> Magic
            # B = unsigned char (1 byte) -> Command
            # I = unsigned int (4 bytes) -> Filename Length (Big Endian >)
            # Q = unsigned long long (8 bytes) -> File Size (Big Endian >)
            header = struct.pack('>BBIQ', MAGIC, CMD_UPLOAD, len(filename_bytes), filesize)

            # 4. Send Header
            sock.sendall(header)
            
            # 5. Send Filename
            sock.sendall(filename_bytes)
            
            # 6. Send File Content
            with open(FILE_TO_SEND, "rb") as f:
                while chunk := f.read(4096):
                    sock.sendall(chunk)
            
            print("File sent successfully!")
            
        except ConnectionRefusedError:
            print("Error: Could not connect. Is the C++ Server running?")
        except Exception as e:
            print(f"An error occurred: {e}")

if __name__ == "__main__":
    create_test_file()
    send_file()