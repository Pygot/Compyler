import sys, os, json, math

def main():
    data = {"argv": sys.argv[1:], "root": getattr(sys, "_MEIPASS", None), "sqrt2": math.sqrt(2)}
    print("hello from compyler")
    print(json.dumps(data, indent=2))
    print("frozen:", getattr(sys, "frozen", False))
    print("cwd:", os.getcwd())
    return 0

if __name__ == "__main__":
    sys.exit(main())
