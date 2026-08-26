import sys, os, json
root = sys._MEIPASS
p = os.path.join(root, "assets", "config.json")
print("data file   :", os.path.exists(p))
print("contents    :", json.load(open(p)))
print("listdir     :", sorted(os.listdir(os.path.join(root, "assets"))))
