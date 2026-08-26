import sys
import tkinter as tk
from tkinter import ttk, font as tkfont

RESULTS = []


def note(k, v):
    RESULTS.append("%s=%s" % (k, v))


def build(root):
    root.title("Compyler tkinter sample")
    root.geometry("420x300")

    style = ttk.Style()
    note("themes", len(style.theme_names()) > 0)

    frame = ttk.Frame(root, padding=8)
    frame.grid(row=0, column=0, sticky="nsew")

    var = tk.StringVar(value="start")
    label = ttk.Label(frame, text="Compyler", font=("Segoe UI", 11))
    label.grid(row=0, column=0, sticky="w")

    entry = ttk.Entry(frame, textvariable=var, width=24)
    entry.grid(row=1, column=0, sticky="w")

    listbox = tk.Listbox(frame, height=5)
    for i in range(12):
        listbox.insert(tk.END, "row %d" % i)
    listbox.grid(row=2, column=0, sticky="w")

    canvas = tk.Canvas(frame, width=200, height=90, background="#202020")
    canvas.grid(row=3, column=0, sticky="w")
    for i in range(24):
        x = i * 8
        canvas.create_line(x, 0, x, 90 - i * 3, fill="#66ccff")
    canvas.create_oval(120, 20, 170, 70, outline="#ffcc66")

    bar = ttk.Progressbar(frame, length=180, maximum=100)
    bar.grid(row=4, column=0, sticky="w")
    bar["value"] = 42

    counter = {"n": 0}

    def bump():
        counter["n"] += 1
        var.set("tick %d" % counter["n"])

    btn = ttk.Button(frame, text="bump", command=bump)
    btn.grid(row=5, column=0, sticky="w")

    for _ in range(5):
        bump()

    root.update_idletasks()
    root.update()

    note("tk_version", tk.TkVersion)
    note("listbox_size", listbox.size())
    note("canvas_items", len(canvas.find_all()))
    note("var", var.get())
    note("bar", int(float(bar["value"])))
    note("label_text", label.cget("text"))
    note("entry_state", str(entry.cget("state")))
    note("families", len(tkfont.families(root)) > 0)
    note("winfo_w", root.winfo_width() > 0)
    note("frozen", getattr(sys, "frozen", False))


def main():
    try:
        root = tk.Tk()
    except Exception as e:
        print("TK_INIT_FAILED", type(e).__name__, e)
        return 2
    try:
        build(root)
    finally:
        root.after(10, root.destroy)
        try:
            root.mainloop()
        except Exception:
            pass
    for line in RESULTS:
        print(line)
    print("OK")
    return 0


sys.exit(main())
