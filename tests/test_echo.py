"""Run the public example as separate server and client processes."""
import pathlib
import socket
import subprocess
import time

root = pathlib.Path(__file__).resolve().parents[1]
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 0))
    port = str(probe.getsockname()[1])
server = subprocess.Popen([str(root / 'build/echo'), 'host', port])
try:
    for _ in range(50):
        result = subprocess.run([str(root / 'build/echo'), '127.0.0.1', port],
                                capture_output=True, text=True, timeout=12)
        if result.returncode == 0:
            break
        assert server.poll() is None, 'server exited before receiving a client'
        time.sleep(.02)
    assert result.returncode == 0, result.stderr
    assert 'echo received' in result.stdout
finally:
    server.terminate()
    assert server.wait(timeout=5) == 0
print('two-process public echo example passed')
