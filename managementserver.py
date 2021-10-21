import socket
import subprocess
import time

TCP_IP = "192.168.1.67"
TCP_PORT = 28008

print("getting ready (making the TCP socket and binding to the port)")
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind((TCP_IP, TCP_PORT))
sock.listen(1)

while True:
    print ("waiting for connection")
    conn, addr = sock.accept()
    print (f"client address: {addr}")
    while True:
        print ("waiting for messages")
        try:
            data = conn.recv(1024)
        except:
            break
        if not data:
            break
        print(f"received: {data}")
        if data == b'start up':
            process = subprocess.Popen("mGBA.exe", close_fds = True,
                                       creationflags=subprocess.CREATE_NEW_CONSOLE)
            time.sleep(3) # wait for the game
            print ("game is ready. responding")
            conn.send(b'game is ready')
        elif data == b'shut down':
            break
    print("client left.")
    conn.close()
    if process:
        process.terminate()
