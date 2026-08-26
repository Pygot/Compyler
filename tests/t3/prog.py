import sys, os, sysconfig

if len(sys.argv) > 1 and sys.argv[1] == "--exit":
    sys.exit(7)

if len(sys.argv) > 1 and sys.argv[1] == "--raise":
    raise RuntimeError("intentional failure")

if len(sys.argv) > 1 and sys.argv[1] == "--mp":
    import multiprocessing as mp

    def work(x):
        return x * x

    if __name__ == "__main__":
        with mp.Pool(2) as p:
            print("pool:", p.map(work, range(6)))
        sys.exit(0)

import pkg_local

print("local import :", pkg_local.compute())
print("prefix       :", sys.prefix)
print("isolated     :", sys.flags.isolated, "no_site:", sys.flags.no_site)
print("PYTHONHOME   :", os.environ.get("PYTHONHOME"))
print("path entries :", len(sys.path))
for p in sys.path:
    print("   ", p.replace(sys._MEIPASS, "<root>"))
print("executable   :", os.path.basename(sys.executable))
print("stdlib       :", sysconfig.get_paths().get("stdlib", "?").replace(sys._MEIPASS, "<root>"))
