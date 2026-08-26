import os
from jinja2 import Template


def template_text():
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "report.txt"), "r", encoding="utf-8") as f:
        return f.read()


def build_report(items, factor):
    t = Template(template_text())
    out = t.render(items=items, factor=factor)
    h = 0
    for ch in out:
        h = (h * 31 + ord(ch)) & 0xFFFFFFFF
    return len(out), h
