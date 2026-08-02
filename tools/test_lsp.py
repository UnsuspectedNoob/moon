import sys
import json
import subprocess
import threading
import time

def send_message(proc, msg):
    body = json.dumps(msg).encode('utf-8')
    header = f"Content-Length: {len(body)}\r\n\r\n".encode('utf-8')
    proc.stdin.write(header + body)
    proc.stdin.flush()

proc = subprocess.Popen(['./moon', '--lsp'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

with open('file.moon', 'r') as f:
    text = f.read()

def read_stdout():
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        if line.startswith(b'Content-Length:'):
            proc.stdout.readline() # \r\n
            length = int(line.split(b':')[1].strip())
            body = proc.stdout.read(length)
            print("Received:", body.decode('utf-8'))

t = threading.Thread(target=read_stdout)
t.daemon = True
t.start()

send_message(proc, {
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {"capabilities": {}}
})

time.sleep(1)

send_message(proc, {
    "jsonrpc": "2.0",
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///home/emrys/moon/file.moon",
            "languageId": "moon",
            "version": 1,
            "text": text
        }
    }
})

time.sleep(2)
proc.terminate()
