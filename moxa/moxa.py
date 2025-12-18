import json
import socket #TCP-Verbindungen aufbauen


def load_config(path="config_example.json"):
    """
    Lädt eine JSON-Datei und gibt (host, ports, timeout_s) zurück.

    Erwartetes Format:
    {
      "moxa": { "host": "192.168.x.y", "ports": [4001,4002], "timeout_s": 1.0 }
    }
    """
    with open(path, "r", encoding="utf-8") as f:
        cfg = json.load(f)

    m = cfg.get("moxa", {})
    host = m.get("host", "")
    ports = m.get("ports", [])
    timeout_s = float(m.get("timeout_s", 1.0))  

    return host, ports, timeout_s


def connect(host, port, timeout_s=1.0):
    """
    Baut eine TCP-Verbindung zur Moxa auf und gibt den Socket zurück.
    """
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout_s)
    s.connect((host, int(port)))
    return s

def exchange(host, port, payload=b"", read_bytes=256, timeout_s=1.0):
    """
    1) verbindet sich zu host:port
    2) sendet payload (wenn nicht leer)
    3) versucht read_bytes zu lesen
    4) gibt Antwortbytes zurück (kann b'' sein)

    Wichtig: b'' ist normal, wenn das Gerät nicht antwortet oder timeout passiert.
    """
    s = connect(host, port, timeout_s=timeout_s)
    try:
        if payload:
            s.sendall(payload)

        chunks = []
        try:
            data = s.recv(read_bytes)
            if data:
                chunks.append(data)

            # kurzer Zusatz-Read (falls Antwort nachtröpfelt)
            s.settimeout(0.2)
            for _ in range(2):
                more = s.recv(read_bytes)
                if not more:
                    break
                chunks.append(more)
        except socket.timeout:
            pass

        return b"".join(chunks)
    finally:
        s.close()



def scan(host, ports, timeout_s=1.0):
    """
    Testet für jede Portnummer, ob connect() klappt.
    Gibt eine Liste von dicts zurück:
      [{"port":4001, "ok":True, "msg":"connect_ok"}, ...]
    """
    results = []
    for p in ports:
        try:
            s = connect(host, p, timeout_s=timeout_s)
            s.close()
            results.append({"port": int(p), "ok": True, "msg": "connect_ok"})
        except Exception as e:
            results.append({"port": int(p), "ok": False, "msg": str(e)})
    return results
