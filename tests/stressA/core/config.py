import json
import os
import sys


def load():
    if getattr(sys, "frozen", False):
        base = os.path.join(sys._MEIPASS, "assets")
    else:
        base = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets")
    with open(os.path.join(base, "config.json"), "r", encoding="utf-8") as f:
        return json.load(f)
