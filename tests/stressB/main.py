import sys
from reportgen import render, imaging, store, edge


def main():
    print("render:", render.build_report(["alpha", "beta", "gamma"], 7))
    print("image:", imaging.checker_hash(64))
    print("store:", store.roundtrip(500))
    print("edge-shadow:", edge.shadowed_len([1, 2, 3, 4]))
    print("edge-monkey:", edge.monkey_sqrt())
    print("edge-negidx:", edge.neg_index_param([10, 20, 30], -2, 3))
    print("edge-bigacc:", edge.big_acc(5))
    print("edge-augmap:", edge.aug_map(300))
    print("edge-condrow:", edge.cond_rows(6, 1), edge.cond_rows(6, 0))
    print("edge-longstr:", edge.long_string())
    print("edge-gen:", edge.gen_sum(50))
    print("edge-clo:", edge.closure_add(9))
    print("edge-star:", edge.star_args(1, 2, 3, 4))
    print("edge-try:", edge.try_div(10, 0), edge.try_div(10, 4))
    return 0


if __name__ == "__main__":
    sys.exit(main())
