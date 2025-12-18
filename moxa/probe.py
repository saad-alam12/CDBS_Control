# tools_probe_channel.py
from moxa import load_config, exchange

host, ports, timeout_s = load_config("config.json")
resp = exchange(host, 4001, payload=b"\r\n", read_bytes=256, timeout_s=timeout_s)
print("Recv:", resp)
print("Recv hex  :", resp.hex())