import sys
from core import engine, textproc, config
from vendorcalc import stats


def main():
    cfg = config.load()
    print("cfg:", cfg["name"], cfg["threshold"])

    print("fib25:", engine.fib(25))
    print("fib-neg:", engine.fib(-7))
    print("grid:", engine.grid_sum(80))
    print("hist:", engine.histogram(200000))
    print("sieve:", engine.sieve(500000))
    print("clamp:", engine.clamp_all(-50, 210))
    print("acc:", engine.acc_overflow(3))
    print("accbig:", engine.acc_overflow(41))
    print("mix:", engine.mixed_ranges(1000))
    print("rows:", engine.row_ops(40))

    print("hash:", textproc.hash_lines(30000))
    print("fmt:", textproc.format_table([3, 1, 4, 1, 5, 9, 2, 6]))
    print("uni:", textproc.unicode_mix())

    print("mean:", stats.mean_of_squares(1000))
    print("norm:", stats.normalized(16))

    if len(sys.argv) > 1 and sys.argv[1] == "--net":
        import requests
        print("requests:", requests.__version__)
    else:
        print("requests: skipped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
