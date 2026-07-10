import socket
import sys

def test_dns():
    # DNS Query for google.com (A record)
    # Transaction ID: 0x1234
    # Flags: 0x0100 (Standard Query)
    # Questions: 1, Answers: 0, Authority: 0, Additional: 0
    query = bytes.fromhex(
        "1234"  # ID
        "0100"  # Flags
        "0001"  # Questions
        "0000"  # Answer RRs
        "0000"  # Authority RRs
        "0000"  # Additional RRs
        "06676f6f676c6503636f6d00"  # google.com
        "0001"  # Type A
        "0001"  # Class IN
    )
    
    server_address = ("127.0.0.1", 5353)
    print(f"Sending DNS query to {server_address}...")
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(5.0)
    
    try:
        sock.sendto(query, server_address)
        data, addr = sock.recvfrom(1024)
        print(f"Received response from {addr}: {data.hex()}")
        
        # Check transaction ID
        resp_id = data[:2].hex()
        print(f"Response Transaction ID: {resp_id}")
        
        # Simple parse IP (usually at the end of A record response)
        # DNS response tail has the IP bytes. For google.com, the last 4 bytes should be the IPv4 address.
        if len(data) >= 32:
            ip = ".".join(str(b) for b in data[-4:])
            print(f"Resolved IP from response: {ip}")
        else:
            print("Response too short to parse IP")
            
    except socket.timeout:
        print("Timeout: No response from DNS proxy server")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        sock.close()

if __name__ == "__main__":
    test_dns()
