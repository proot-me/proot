import socket
import sys
import os
import time

HOST = '::1'
PORT = 6432

pid = os.fork()
addrs = socket.getaddrinfo(HOST, PORT, socket.AF_INET6, 0, socket.SOL_TCP)[0][-1]
#addrs = (HOST, PORT)
print addrs

# Server
if pid:

    # Create syscall
    sock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)

    if len(sys.argv) > 1:
        time.sleep(int(sys.argv[1]))

    # Bind syscall
    print "Server bind"
    sock.bind(addrs)

    # Listen syscall
    print "Server listen"
    sock.listen(1)

    try:
        # Accept syscall
        print "Server accept"
        conn, addr = sock.accept()

        while True:
            data = conn.recv(1024)
            if not data:
                break
            else:
                #if len(sys.argv) > 4:
                #    with open("fakeoutput" + sys.argv[4] + ".txt", "a") as testfile:
                #        testfile.write(data)
                #    os.remove("fakeoutput" + sys.argv[4] + ".txt")
                print "Server data received : " + data
                break
    finally:
        # Close syscall
        sock.close()

# Client
else:
    # Socket syscall
    sock = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)

    if len(sys.argv) > 2:
        time.sleep(int(sys.argv[2]))

    try:
        # Connect syscall
        print "Client connect"
        sock.connect(addrs)
    except socket.error, msg:
        print >>sys.stderr, msg
        sys.exit(1)

    if len(sys.argv) > 3:
        time.sleep(int(sys.argv[3]))

    try:
        # send Syscall
        sock.send("test " + sys.argv[4])
    finally:
        sock.close()
