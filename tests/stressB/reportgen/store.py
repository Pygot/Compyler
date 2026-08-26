import csv
import io
import json
import sqlite3


def roundtrip(n):
    con = sqlite3.connect(":memory:")
    cur = con.cursor()
    cur.execute("CREATE TABLE t (k INTEGER PRIMARY KEY, v TEXT)")
    for i in range(n):
        cur.execute("INSERT INTO t VALUES (?, ?)", (i, f"row{i % 13}"))
    con.commit()
    total = cur.execute("SELECT COUNT(*), SUM(k) FROM t").fetchone()

    sio = io.StringIO()
    w = csv.writer(sio, lineterminator="\n")
    for row in cur.execute("SELECT * FROM t WHERE k < 5"):
        w.writerow(row)
    blob = json.dumps({"total": total, "head": sio.getvalue()}, sort_keys=True)
    con.close()
    return len(blob), sum(ord(c) for c in blob) % 100000
