import json
import subprocess
import sys


def send_message(proc, msg):
    content = json.dumps(msg)
    header = f"Content-Length: {len(content)}\r\n\r\n"
    proc.stdin.write(header.encode("utf-8"))
    proc.stdin.write(content.encode("utf-8"))
    proc.stdin.flush()


def read_message(proc):
    content_length = 0
    while True:
        line = proc.stdout.readline().decode("utf-8")
        if not line:
            return None
        if line == "\r\n":
            break
        if line.startswith("Content-Length: "):
            content_length = int(line[16:].strip())

    if content_length > 0:
        content = proc.stdout.read(content_length).decode("utf-8")
        return json.loads(content)
    return None


if __name__ == "__main__":
    with open("benchmarks/sieve.moon", "r") as f:
        source = f.read()

    proc = subprocess.Popen(
        ["./moon", "--lsp"], stdin=subprocess.PIPE, stdout=subprocess.PIPE
    )

    # Initialize
    send_message(
        proc, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}}
    )
    print("Sent initialize", file=sys.stderr)
    msg = read_message(proc)
    print("Received:", msg, file=sys.stderr)

    # Send didOpen
    send_message(
        proc,
        {
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file:///benchmarks/sieve.moon",
                    "languageId": "moon",
                    "version": 1,
                    "text": source,
                }
            },
        },
    )
    print("Sent didOpen", file=sys.stderr)

    # Send formatting request
    send_message(
        proc,
        {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "textDocument/formatting",
            "params": {"textDocument": {"uri": "file:///benchmarks/sieve.moon"}},
        },
    )
    print("Sent formatting", file=sys.stderr)
    while True:
        msg = read_message(proc)
        if msg and msg.get("id") == 2:
            print("Received Formatting:", json.dumps(msg, indent=2))
            break
        elif msg:
            print("Received Other:", json.dumps(msg, indent=2), file=sys.stderr)

    proc.terminate()
