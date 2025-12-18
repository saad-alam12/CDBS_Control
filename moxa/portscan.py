# tools_scan_moxa.py
from moxa import load_config, scan

host, ports, timeout_s = load_config("config.json")
if not host or host == "einfügen":
    raise SystemExit("Ip in config eintragen")

results = scan(host, ports, timeout_s=timeout_s)
for r in results:
    print(r["port"], "OK" if r["ok"] else "FAIL", r["msg"])
