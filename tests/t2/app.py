import sys, time

t0 = time.perf_counter()

import numpy as np
from scapy.all import IP, TCP, Ether, Raw
import requests

print("python  :", sys.version.split()[0])
print("frozen  :", getattr(sys, "frozen", False))
print("numpy   :", np.__version__)

a = np.arange(12, dtype=np.float64).reshape(3, 4)
print("matmul  :", float((a @ a.T @ np.ones(3)).sum()))
print("fft     :", round(float(abs(np.fft.fft(a[0])[1])), 6))

pkt = Ether() / IP(dst="1.2.3.4", src="10.0.0.1") / TCP(dport=443, flags="S") / Raw(b"compyler")
print("scapy   :", pkt.summary())
print("pkt len :", len(bytes(pkt)))
print("ip ver  :", pkt[IP].version, "dport", pkt[TCP].dport)

s = requests.Session()
print("requests:", requests.__version__, type(s).__name__)

print("import+run in %.3fs" % (time.perf_counter() - t0))
