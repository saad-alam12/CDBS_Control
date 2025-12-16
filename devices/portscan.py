# tools_scan_moxa.py
from moxa import load_config, scan

host, ports, timeout_s = load_config("config.json")
if not host or host == "einfügen":
    raise SystemExit("Setze moxa.host in config.json auf die IP der Moxa.")

results = scan(host, ports, timeout_s=timeout_s)
for r in results:
    print(r["port"], "OK" if r["ok"] else "FAIL", r["msg"])
