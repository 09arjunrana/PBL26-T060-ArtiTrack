#!/usr/bin/env python3
"""
artitrack_gui.py - a GUI for ArtiTrack that needs ZERO changes to the C/C++ code.

THE ONE IDEA (say this first in your presentation)
--------------------------------------------------
The C/C++ program is a conversation: it prints a menu or a question, waits for a line
of text, then prints the next thing. This GUI starts that SAME program (artitrack.exe)
as a hidden child process and plays the part of the person at the keyboard:

    button click  ->  GUI types lines into the program's stdin
    program output <-  GUI reads the program's stdout and shows it in the window

All the real work (SHA-256 login, hash table, merge sort, graph BFS/DFS, undo stack,
restoration heap, file saving) still runs inside your C/C++ code. The GUI never
re-implements any of it.

FILE LAYOUT (top to bottom)
---------------------------
  1. Session        - starts the program, sends one line, waits for the next prompt
  2. ArtiTrackClient- one method per feature ("add artifact" = the list of lines to type)
  3. App (Tkinter)  - windows, forms and buttons; calls the client
"""
import os
import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk


# =====================================================================================
# 1. SESSION - talking to the program through pipes
# =====================================================================================
MENU_PROMPTS = ("Choice: ", "Enter your choice: ")        # the two "pick a number" prompts

# The program prints one of these headers right before it asks for a menu choice. That is
# how the GUI knows WHICH menu the program is currently sitting in.
MENU_HEADERS = {
    "===== Artifact Menu =====": "artifact",
    "===== Movement & Restoration =====": "movement",
    "===== Search, Sort & Reports =====": "search",
    "\n1. Register": "top",                                # login menu has no ===== header
}


class Reply:
    """Everything the program printed after we sent one line, split into useful parts."""

    def __init__(self, text, closed=False):
        self.text = text                                   # raw output
        self.closed = closed                               # True if the program exited
        cut = text.rfind("\n")
        self.body = text[: cut + 1]                        # all complete lines
        self.prompt = text[cut + 1:]                       # unfinished last line = the question

        # Which menu are we in, and where does the (repeated) menu text begin?
        self.screen, menu_start = None, -1
        if self.prompt in MENU_PROMPTS:
            for header, name in MENU_HEADERS.items():
                at = text.rfind(header)
                if at > menu_start:
                    menu_start, self.screen = at, name
        # 'content' = the useful output with the menu text chopped off the end
        self.content = (text[:menu_start] if menu_start >= 0 else self.body).strip()


class ProtocolError(Exception):
    """The program asked something other than what we expected (or refused our input)."""

    def __init__(self, message, reply):
        super().__init__(message)
        self.reply = reply


class Session:
    QUIET = 0.05        # seconds of silence after a ': ' ending before we call it a prompt
    TIMEOUT = 5.0       # give up if the program says nothing for this long

    def __init__(self, exe, on_event=None):
        self.exe = exe
        self.workdir = os.path.dirname(os.path.abspath(exe))   # data files live next to the exe
        self.on_event = on_event or (lambda kind, text: None)
        self.proc = None
        self.chunks = queue.Queue()
        self.last = None                                       # most recent Reply

    # ---- start / stop ----
    def start(self):
        flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0   # no black console window
        self.proc = subprocess.Popen(
            [self.exe], cwd=self.workdir,                       # cwd matters: artifacts.txt etc.
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            bufsize=0, creationflags=flags)
        self.chunks = queue.Queue()
        # A helper thread keeps reading the pipe so the GUI never freezes waiting on it.
        threading.Thread(target=self._pump, args=(self.proc, self.chunks), daemon=True).start()
        self.on_event("note", "Started %s as a hidden child process." % os.path.basename(self.exe))
        self.last = self._read_reply()
        return self.last

    @staticmethod
    def _pump(proc, chunks):
        fd = proc.stdout.fileno()
        while True:
            try:
                data = os.read(fd, 4096)
            except OSError:
                data = b""
            chunks.put(data or None)                            # None = pipe closed
            if not data:
                return

    def stop(self):
        if not self.proc:
            return
        try:
            self.proc.stdin.close()                             # program sees end-of-input and exits
            self.proc.wait(timeout=1)
        except Exception:
            self.proc.kill()
        self.proc = None

    # ---- the core: send ONE line, then read until the program asks for the next one ----
    def send(self, line):
        self.on_event("send", line)
        try:
            self.proc.stdin.write((line + "\n").encode("utf-8"))
            self.proc.stdin.flush()
        except (OSError, ValueError) as err:
            raise ProtocolError("program is not running: %s" % err, Reply("", closed=True))
        self.last = self._read_reply()
        return self.last

    def _read_reply(self):
        """A prompt is output that ENDS with ': ' (no newline) and then goes quiet."""
        text, deadline = "", time.time() + self.TIMEOUT
        while True:
            looks_like_prompt = text.endswith(": ")
            try:
                chunk = self.chunks.get(timeout=self.QUIET if looks_like_prompt else 0.2)
            except queue.Empty:
                if looks_like_prompt:
                    break                                       # quiet + ends in ': ' -> it is waiting
                if time.time() > deadline:
                    raise TimeoutError("the program stopped answering")
                continue
            if chunk is None:
                reply = Reply(text, closed=True)
                self.on_event("recv", text)
                return reply
            text += chunk.decode("utf-8", errors="replace").replace("\r\n", "\n")   # Windows newlines
        self.on_event("recv", text)
        return Reply(text)

    def flow(self, steps):
        """steps = [(text the current prompt must contain, line to send), ...]

        Before every answer we CHECK the program is really asking what we think it is.
        If not (duplicate ID, 'Artifact not found', ...) we stop and raise ProtocolError.
        """
        reply = self.last
        for expected, value in steps:
            if expected not in reply.prompt:
                raise ProtocolError("expected a prompt containing %r but got %r"
                                    % (expected, reply.prompt), reply)
            reply = self.send(value)
        return reply


# =====================================================================================
# 2. CLIENT - one method per feature = the keystrokes a human would type
# =====================================================================================
TYPES = ["Pottery", "Coin", "Ancient Coin", "Weapon", "Sculpture", "Manuscript"]   # menu 1..6
METALS = ["Gold", "Silver", "Bronze", "Copper"]                                    # menu 1..4
INDEX_LABEL = {1: "Porosity Index (0.0 - 1.0)", 4: "Rust Index (0.0 - 1.0)",
               5: "Crack Index (0.0 - 1.0)", 6: "Decay Index (0.0 - 1.0)"}
INDEX_PROMPT = {1: "Porosity Index", 4: "Rust Index", 5: "Crack Index", 6: "Decay Index"}

# Which keystroke leads from one screen to another
ROUTE = {("artifact", "movement"): "5", ("artifact", "search"): "4",
         ("movement", "artifact"): "6", ("search", "artifact"): "6"}


class ArtiTrackClient:
    def __init__(self, session):
        self.s = session
        self.user = self.password = None

    def note(self, text):                                       # plain-English explanation for the log
        self.s.on_event("note", text)

    # ---- program lifecycle ----
    def start(self):
        self.s.start()

    def close(self):
        self.s.stop()

    def restart(self):
        """Used if we ever get out of step: kill it, start fresh, log in again."""
        self.s.stop()
        self.s.start()
        if self.user:
            self.login(self.user, self.password)

    # ---- screens ----
    def goto(self, target):
        """Walk through the menus until the program is on the screen we want."""
        while self.s.last.screen != target:
            here = self.s.last.screen
            if here in ("movement", "search"):
                key = ROUTE[(here, "artifact")]                 # go back up first
            elif here == "artifact":
                key = ROUTE[("artifact", target)]
            else:
                raise ProtocolError("not logged in", self.s.last)
            self.note("Menu navigation: typing %s to leave the '%s' menu." % (key, here))
            reply = self.s.flow([("Enter your choice: ", key)])
            if reply.screen not in (target, "artifact"):
                raise ProtocolError("could not open the %s menu" % target, reply)

    # ---- login module (m0) ----
    def register(self, user, password):
        self.note("Login menu option 1 = Register. Then the program asks for username and password.")
        r = self.s.flow([("Choice: ", "1"), ("Enter username: ", user), ("Enter password: ", password)])
        return r.content

    def login(self, user, password):
        self.note("Login menu option 2 = Login. The C++ code hashes the password (salted SHA-256) "
                  "and compares it with users.txt - the GUI never sees the hash.")
        try:
            r = self.s.flow([("Choice: ", "2"), ("Enter username: ", user),
                             ("Enter password: ", password)])
        except ProtocolError as err:
            return False, err.reply.content
        if r.screen == "artifact":
            self.user, self.password = user, password
            return True, r.content
        return False, r.content

    def logout(self):
        self.goto("artifact")
        self.note("Artifact menu option 6 = Logout.")
        self.s.flow([("Enter your choice: ", "6")])
        self.user = self.password = None

    # ---- artifact module (m1 / m2) ----
    def display_all(self):
        self.goto("artifact")
        self.note("Artifact menu option 2 = Display All. Just sending '2' and showing what it prints.")
        return self.s.flow([("Enter your choice: ", "2")]).content

    def search_id(self, artifact_id):
        self.goto("artifact")
        self.note("Artifact menu option 3 = Search by ID, then type the ID.")
        return self.s.flow([("Enter your choice: ", "3"),
                            ("Enter Artifact ID to search: ", artifact_id)]).content

    def add_artifact(self, kind, aid, name, age, cond, loc, extra=None, metal=None, dynasty=None):
        """kind 1..6. The questions asked depend on the type, exactly like in main.cpp."""
        self.goto("artifact")
        self.note("Artifact menu option 1 = Add. The program now asks ~7-8 questions in a fixed "
                  "order (see addArtifact() in main.cpp); we answer them one by one.")
        steps = [("Enter your choice: ", "1"), ("Select type: ", str(kind)),
                 ("Enter Artifact ID", aid), ("Enter Name: ", name),
                 ("Enter Estimated Age", str(age)), ("Enter Condition Score", str(cond)),
                 ("Enter Current Location: ", loc)]
        if kind in INDEX_PROMPT:
            steps.append((INDEX_PROMPT[kind], str(extra)))
        else:
            steps.append(("Select metal: ", str(METALS.index(metal) + 1)))
            if kind == 3:
                steps.append(("Enter Dynasty", dynasty))
        return self.s.flow(steps).content

    # ---- movement module (m4.c, pure C) ----
    def move(self, aid, new_loc, reason):
        self.goto("movement")
        self.note("Movement menu option 1. This runs the PURE C code in m4.c, which calls back into "
                  "C++ through bridge.cpp to change the location.")
        return self.s.flow([("Enter your choice: ", "1"), ("Enter Artifact ID to move: ", aid),
                            ("Enter New Location: ", new_loc),
                            ("Enter Reason for movement: ", reason)]).content

    def history(self, aid):
        self.goto("movement")
        self.note("Movement menu option 2 = history of one artifact (walks the linked list in m4.c).")
        return self.s.flow([("Enter your choice: ", "2"), ("Enter Artifact ID: ", aid)]).content

    def full_log(self):
        self.goto("movement")
        self.note("Movement menu option 3 = print the whole movement log.")
        return self.s.flow([("Enter your choice: ", "3")]).content

    def undo_begin(self):
        """Returns (text, needs_confirmation). The C code itself asks 'Undo it? (1=yes, 2=no)'."""
        self.goto("movement")
        self.note("Movement menu option 4 = Undo. The undo STACK is a static variable inside the "
                  "running program - that is why the GUI must keep ONE process alive.")
        r = self.s.flow([("Enter your choice: ", "4")])
        if "Undo it?" in r.prompt:
            return r.body.strip(), True
        return r.content, False

    def undo_answer(self, yes):
        r = self.s.flow([("Undo it?", "1" if yes else "2")])
        return r.content

    def restoration_queue(self, count):
        self.goto("movement")
        self.note("Movement menu option 5 = restoration priority queue (a binary max-heap in m4.c).")
        return self.s.flow([("Enter your choice: ", "5"),
                            ("How many artifacts", str(count))]).content

    # ---- search / sort / reports module (m3) ----
    def _search_menu(self, steps, why):
        self.goto("search")
        self.note(why)
        return self.s.flow(steps).content

    def find_hash(self, aid):
        return self._search_menu([("Enter your choice: ", "1"), ("Enter Artifact ID: ", aid)],
                                 "Search menu option 1 = lookup through the C hash table (m3_core.c).")

    def filter_type(self, n):
        return self._search_menu([("Enter your choice: ", "2"), ("Select filter: ", "1"),
                                  ("Select type: ", str(n))], "Search menu option 2 -> filter by type.")

    def filter_era(self, n):
        return self._search_menu([("Enter your choice: ", "2"), ("Select filter: ", "2"),
                                  ("Select era: ", str(n))], "Search menu option 2 -> filter by era.")

    def filter_condition(self, lo, hi):
        return self._search_menu([("Enter your choice: ", "2"), ("Select filter: ", "3"),
                                  ("Minimum condition", str(lo)), ("Maximum condition", str(hi))],
                                 "Search menu option 2 -> filter by condition range.")

    def sorted_listing(self, key, order):
        return self._search_menu([("Enter your choice: ", "3"), ("Sort by: ", str(key)),
                                  ("Order: ", str(order))],
                                 "Search menu option 3 = sorted listing (stable merge sort in C).")

    def report(self, n):
        return self._search_menu([("Enter your choice: ", "4"), ("Select report: ", str(n))],
                                 "Search menu option 4 = reports.")


# =====================================================================================
# 3. GUI - windows, forms, buttons
# =====================================================================================
MONO = ("Consolas", 11)

# --- colour palette (change these to re-theme the whole app) ---
BG      = "#F5F3FF"     # window background (soft lavender)
CARD    = "#FFFFFF"     # login card
INDIGO  = "#4338CA"     # header bar
PURPLE  = "#7C3AED"     # main buttons / selected tab
TEXT    = "#1E1B4B"     # normal text
NAVY    = "#0F172A"     # output panel background
MINT    = "#6EE7B7"     # output panel text
ERAS = ["Modern  (age under 200)", "Early Modern  (200 - 499)",
        "Medieval  (500 - 1499)", "Ancient  (1500 and over)"]
FILTER_TYPES = ["Artifact (generic)"] + TYPES


def need_text(value, label):
    """Same rule as readLine() in m0.cpp: not empty, no '|' (it is the file separator)."""
    value = value.strip()
    if not value:
        raise ValueError("%s cannot be empty." % label)
    if "|" in value:
        raise ValueError("%s cannot contain the '|' character." % label)
    return value


def need_word(value, label):
    """Same rule as readToken(): one word, no spaces."""
    value = need_text(value, label)
    if len(value.split()) != 1:
        raise ValueError("%s must be a single word (no spaces)." % label)
    return value


def need_int(value, label, lo, hi):
    try:
        number = int(value)
    except ValueError:
        raise ValueError("%s must be a whole number." % label)
    if not lo <= number <= hi:
        raise ValueError("%s must be between %d and %d." % (label, lo, hi))
    return number


def need_float(value, label, lo, hi):
    try:
        number = float(value)
    except ValueError:
        raise ValueError("%s must be a number." % label)
    if not lo <= number <= hi:
        raise ValueError("%s must be between %g and %g." % (label, lo, hi))
    return number


def find_exe():
    here = os.path.dirname(os.path.abspath(__file__))
    for folder in (here, os.getcwd()):
        for name in ("artitrack.exe", "artitrack"):
            path = os.path.join(folder, name)
            if os.path.isfile(path):
                return path
    return None


class App(tk.Tk):
    def __init__(self, exe):
        super().__init__()
        self.title("ArtiTrack - GUI wrapper")
        self.geometry("1020x780")
        self.minsize(900, 700)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

        self.client = ArtiTrackClient(Session(exe, on_event=self.log_event))
        self.tab_screens = ["artifact", "movement", "search"]
        self.build_layout()
        self.client.start()
        self.show_login()

    # ------------------------------------------------------------------ layout
    def setup_style(self):
        """All colours and fonts live here. ttk 'clam' theme lets us colour buttons on Windows."""
        st = ttk.Style(self)
        st.theme_use("clam")
        self.configure(bg=BG)
        st.configure(".", background=BG, foreground=TEXT, font=("Segoe UI", 10))
        st.configure("Header.TFrame", background=INDIGO)
        st.configure("Card.TFrame", background=CARD)
        st.configure("Card.TLabel", background=CARD)
        st.configure("Title.TLabel", background=INDIGO, foreground="white", font=("Segoe UI", 20, "bold"))
        st.configure("Sub.TLabel", background=INDIGO, foreground="#C7D2FE", font=("Segoe UI", 9))
        st.configure("Who.TLabel", background=INDIGO, foreground="#FDE68A", font=("Segoe UI", 11, "bold"))
        st.configure("Heading.TLabel", background=CARD, foreground=INDIGO, font=("Segoe UI", 14, "bold"))
        st.configure("TLabelframe", background=BG, bordercolor="#C4B5FD")
        st.configure("TLabelframe.Label", background=BG, foreground=PURPLE, font=("Segoe UI", 10, "bold"))
        st.configure("TNotebook", background=BG, borderwidth=0)
        st.configure("TNotebook.Tab", background="#DDD6FE", foreground=INDIGO, padding=(18, 8),
                     font=("Segoe UI", 10, "bold"))
        st.map("TNotebook.Tab", background=[("selected", PURPLE), ("active", "#C4B5FD")],
               foreground=[("selected", "white")])
        st.configure("TEntry", fieldbackground="white", bordercolor="#C4B5FD", padding=4)
        st.map("TEntry", bordercolor=[("focus", PURPLE)])
        st.configure("TCombobox", fieldbackground="white", bordercolor="#C4B5FD", arrowcolor=PURPLE, padding=3)
        # buttons: default = purple, plus green / pink / sky variants
        for name, base, hover in (("TButton", PURPLE, "#6D28D9"), ("Go.TButton", "#10B981", "#059669"),
                                  ("Danger.TButton", "#F43F5E", "#E11D48"), ("Alt.TButton", "#0EA5E9", "#0284C7")):
            st.configure(name, background=base, foreground="white", borderwidth=0, padding=(14, 6),
                         font=("Segoe UI", 10, "bold"), focuscolor=base)
            st.map(name, background=[("pressed", hover), ("active", hover)])

    def build_layout(self):
        self.setup_style()
        self.columnconfigure(0, weight=1)
        self.rowconfigure(2, weight=1)

        # --- coloured header bar ---
        head = ttk.Frame(self, style="Header.TFrame", padding=(18, 12))
        head.grid(row=0, column=0, sticky="ew")
        titles = ttk.Frame(head, style="Header.TFrame")
        titles.pack(side="left")
        ttk.Label(titles, text="ArtiTrack", style="Title.TLabel").pack(anchor="w")
        ttk.Label(titles, text="Artifact Movement & Preservation Tracker", style="Sub.TLabel").pack(anchor="w")
        self.who = ttk.Label(head, text="", style="Who.TLabel")
        self.logout_btn = ttk.Button(head, text="Logout", style="Danger.TButton", command=self.do_logout)

        self.stage = ttk.Frame(self, padding=(16, 14))
        self.stage.grid(row=1, column=0, sticky="ew")

        out_box = ttk.LabelFrame(self, text=" Program output ")
        out_box.grid(row=2, column=0, sticky="nsew", padx=16, pady=(0, 16))
        self.output = scrolledtext.ScrolledText(out_box, height=10, font=MONO, state="disabled", wrap="none",
                                                background=NAVY, foreground=MINT, insertbackground=MINT,
                                                relief="flat", padx=10, pady=8)
        self.output.pack(fill="both", expand=True, padx=4, pady=4)

        # --- login card ---
        self.login_frame = ttk.Frame(self.stage, style="Card.TFrame", padding=24)
        self.user_var, self.pass_var = tk.StringVar(), tk.StringVar()
        ttk.Label(self.login_frame, text="Welcome - log in or create an account",
                  style="Heading.TLabel").grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 12))
        ttk.Label(self.login_frame, text="Username", style="Card.TLabel").grid(row=1, column=0, sticky="e", padx=4, pady=4)
        ttk.Entry(self.login_frame, textvariable=self.user_var, width=26).grid(row=1, column=1, pady=4)
        ttk.Label(self.login_frame, text="Password", style="Card.TLabel").grid(row=2, column=0, sticky="e", padx=4, pady=4)
        pass_entry = ttk.Entry(self.login_frame, textvariable=self.pass_var, width=26, show="*")
        pass_entry.grid(row=2, column=1, pady=4)
        pass_entry.bind("<Return>", lambda e: self.do_login())
        ttk.Button(self.login_frame, text="Login", command=self.do_login).grid(row=1, column=2, padx=10)
        ttk.Button(self.login_frame, text="Register", style="Alt.TButton",
                   command=self.do_register).grid(row=2, column=2, padx=10)

        # --- main notebook ---
        self.notebook = ttk.Notebook(self.stage)
        self.build_artifacts_tab()
        self.build_movement_tab()
        self.build_search_tab()
        self.notebook.bind("<<NotebookTabChanged>>", self.on_tab_changed)

    def build_artifacts_tab(self):
        tab = ttk.Frame(self.notebook, padding=8)
        self.notebook.add(tab, text="Artifacts")

        row = ttk.Frame(tab)
        row.pack(fill="x")
        ttk.Button(row, text="Display all artifacts", command=lambda: self.run(self.client.display_all)).pack(side="left")
        ttk.Label(row, text="   Search by ID:").pack(side="left")
        self.search_id_var = tk.StringVar()
        ttk.Entry(row, textvariable=self.search_id_var, width=12).pack(side="left", padx=4)
        ttk.Button(row, text="Search", command=self.do_search_id).pack(side="left")

        form = ttk.LabelFrame(tab, text=" Add artifact ", padding=8)
        form.pack(fill="x", pady=(8, 0))
        self.f = {k: tk.StringVar() for k in ("id", "name", "age", "cond", "loc", "extra", "dynasty")}
        self.type_var, self.metal_var = tk.StringVar(value=TYPES[0]), tk.StringVar(value=METALS[0])

        ttk.Label(form, text="Type").grid(row=0, column=0, sticky="e", padx=4, pady=2)
        type_box = ttk.Combobox(form, textvariable=self.type_var, values=TYPES, state="readonly", width=16)
        type_box.grid(row=0, column=1, sticky="w")
        type_box.bind("<<ComboboxSelected>>", lambda e: self.update_type_fields())
        for r, (key, label) in enumerate([("id", "Artifact ID"), ("name", "Name"),
                                          ("age", "Estimated age (years)"),
                                          ("cond", "Condition (0-10)"), ("loc", "Current location")], start=1):
            ttk.Label(form, text=label).grid(row=r, column=0, sticky="e", padx=4, pady=2)
            ttk.Entry(form, textvariable=self.f[key], width=30).grid(row=r, column=1, sticky="w")

        # fields that depend on the type; shown/hidden by update_type_fields()
        self.extra_label = ttk.Label(form)
        self.extra_entry = ttk.Entry(form, textvariable=self.f["extra"], width=12)
        self.metal_label = ttk.Label(form, text="Metal")
        self.metal_box = ttk.Combobox(form, textvariable=self.metal_var, values=METALS, state="readonly", width=12)
        self.dynasty_label = ttk.Label(form, text="Dynasty")
        self.dynasty_entry = ttk.Entry(form, textvariable=self.f["dynasty"], width=20)
        ttk.Button(form, text="Add artifact", style="Go.TButton", command=self.do_add).grid(row=8, column=1, sticky="w", pady=6)
        self.type_form = form
        self.update_type_fields()

    def update_type_fields(self):
        for w in (self.extra_label, self.extra_entry, self.metal_label, self.metal_box,
                  self.dynasty_label, self.dynasty_entry):
            w.grid_remove()
        kind = TYPES.index(self.type_var.get()) + 1
        if kind in INDEX_LABEL:
            self.extra_label.config(text=INDEX_LABEL[kind])
            self.extra_label.grid(row=6, column=0, sticky="e", padx=4, pady=2)
            self.extra_entry.grid(row=6, column=1, sticky="w")
        else:
            self.metal_label.grid(row=6, column=0, sticky="e", padx=4, pady=2)
            self.metal_box.grid(row=6, column=1, sticky="w")
            if kind == 3:
                self.dynasty_label.grid(row=7, column=0, sticky="e", padx=4, pady=2)
                self.dynasty_entry.grid(row=7, column=1, sticky="w")

    def build_movement_tab(self):
        tab = ttk.Frame(self.notebook, padding=8)
        self.notebook.add(tab, text="Movement & Restoration")
        self.mv = {k: tk.StringVar() for k in ("id", "to", "why", "hist")}

        move = ttk.LabelFrame(tab, text=" Move an artifact ", padding=8)
        move.pack(fill="x")
        for r, (key, label) in enumerate([("id", "Artifact ID"), ("to", "New location"),
                                          ("why", "Reason for movement")]):
            ttk.Label(move, text=label).grid(row=r, column=0, sticky="e", padx=4, pady=2)
            ttk.Entry(move, textvariable=self.mv[key], width=34).grid(row=r, column=1, sticky="w")
        ttk.Button(move, text="Move", style="Go.TButton", command=self.do_move).grid(row=3, column=1, sticky="w", pady=4)

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=(8, 0))
        ttk.Label(row, text="History of ID:").pack(side="left")
        ttk.Entry(row, textvariable=self.mv["hist"], width=12).pack(side="left", padx=4)
        ttk.Button(row, text="Show history", command=self.do_history).pack(side="left")
        ttk.Button(row, text="Full movement log", command=lambda: self.run(self.client.full_log)).pack(side="left", padx=12)
        ttk.Button(row, text="Undo last move", style="Danger.TButton", command=self.do_undo).pack(side="left")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=(8, 0))
        ttk.Label(row, text="Restoration queue - how many to list (1-50):").pack(side="left")
        self.queue_var = tk.StringVar(value="10")
        ttk.Entry(row, textvariable=self.queue_var, width=6).pack(side="left", padx=4)
        ttk.Button(row, text="Show queue", command=self.do_queue).pack(side="left")

    def build_search_tab(self):
        tab = ttk.Frame(self.notebook, padding=8)
        self.notebook.add(tab, text="Search & Reports")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Find by ID (hash table):", width=26).pack(side="left")
        self.hash_var = tk.StringVar()
        ttk.Entry(row, textvariable=self.hash_var, width=12).pack(side="left", padx=4)
        ttk.Button(row, text="Find", command=self.do_find_hash).pack(side="left")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Filter by type:", width=26).pack(side="left")
        self.ft_box = ttk.Combobox(row, values=FILTER_TYPES, state="readonly", width=20)
        self.ft_box.current(0)
        self.ft_box.pack(side="left", padx=4)
        ttk.Button(row, text="Filter", command=lambda: self.run(self.client.filter_type, self.ft_box.current() + 1)).pack(side="left")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Filter by era:", width=26).pack(side="left")
        self.fe_box = ttk.Combobox(row, values=ERAS, state="readonly", width=26)
        self.fe_box.current(0)
        self.fe_box.pack(side="left", padx=4)
        ttk.Button(row, text="Filter", command=lambda: self.run(self.client.filter_era, self.fe_box.current() + 1)).pack(side="left")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Filter by condition range:", width=26).pack(side="left")
        self.lo_var, self.hi_var = tk.StringVar(value="0"), tk.StringVar(value="10")
        ttk.Entry(row, textvariable=self.lo_var, width=6).pack(side="left", padx=(4, 2))
        ttk.Label(row, text="to").pack(side="left")
        ttk.Entry(row, textvariable=self.hi_var, width=6).pack(side="left", padx=2)
        ttk.Button(row, text="Filter", command=self.do_filter_condition).pack(side="left", padx=4)

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Sorted listing (merge sort):", width=26).pack(side="left")
        self.sk_box = ttk.Combobox(row, values=["Age", "Condition", "Priority"], state="readonly", width=12)
        self.sk_box.current(2)
        self.sk_box.pack(side="left", padx=4)
        self.so_box = ttk.Combobox(row, values=["Ascending", "Descending"], state="readonly", width=12)
        self.so_box.current(1)
        self.so_box.pack(side="left", padx=4)
        ttk.Button(row, text="Sort", command=lambda: self.run(
            self.client.sorted_listing, self.sk_box.current() + 1, self.so_box.current() + 1)).pack(side="left")

        row = ttk.Frame(tab)
        row.pack(fill="x", pady=2)
        ttk.Label(row, text="Report:", width=26).pack(side="left")
        self.rp_box = ttk.Combobox(row, values=["Urgent care (risk bands)", "Risky storage (by location)",
                                                "Movement frequency"], state="readonly", width=28)
        self.rp_box.current(0)
        self.rp_box.pack(side="left", padx=4)
        ttk.Button(row, text="Show report", command=lambda: self.run(self.client.report, self.rp_box.current() + 1)).pack(side="left")

    # ------------------------------------------------------------------ screens
    def show_login(self):
        self.notebook.pack_forget()
        self.logout_btn.pack_forget()
        self.who.pack_forget()
        self.login_frame.pack(anchor="w", pady=4)

    def show_main(self, user):
        self.login_frame.pack_forget()
        self.notebook.pack(fill="x")
        self.logout_btn.pack(side="right")
        self.who.config(text="Logged in as %s" % user)
        self.who.pack(side="right", padx=16)
        self.notebook.select(0)

    # ------------------------------------------------------------------ output / log
    def set_output(self, text):
        self.output.config(state="normal")
        self.output.delete("1.0", "end")
        self.output.insert("1.0", text if text else "(the program printed nothing)")
        self.output.config(state="disabled")

    def log_event(self, kind, text):
        """Hook for the Session/Client explanations. Intentionally shows nothing in the window."""
        pass

    # ------------------------------------------------------------------ running actions
    def run(self, func, *args):
        """Call a client method and show the result. All error handling lives here."""
        try:
            text = func(*args)
        except ProtocolError as err:
            if err.reply.screen:                               # program is back at a menu: it just
                self.set_output(err.reply.content or str(err))  # declined (e.g. 'Artifact not found.')
            else:                                              # we are out of step: reset cleanly
                self.recover(str(err))
            return None
        except (TimeoutError, OSError) as err:
            self.recover(str(err))
            return None
        self.set_output(text)
        return text

    def recover(self, why):
        self.log_event("note", "Out of step with the program (%s). Restarting it and logging in again." % why)
        messagebox.showwarning("Resynchronising",
                               "The GUI and the program got out of step, so the program was restarted.\n\n"
                               "(Saved data is safe - it is written to the .txt files after every action. "
                               "Only the in-memory undo history was lost.)")
        try:
            self.client.restart()
        except Exception as err:
            messagebox.showerror("Error", "Could not restart the program: %s" % err)
            self.destroy()

    def check(self, builder):
        """Run a validation function; show a friendly message instead of crashing on bad input."""
        try:
            return builder()
        except ValueError as err:
            messagebox.showerror("Check your input", str(err))
            return None

    # ------------------------------------------------------------------ handlers
    def on_tab_changed(self, _event):
        if self.client.user:
            index = self.notebook.index(self.notebook.select())
            self._goto_tab(index)

    def _goto_tab(self, index):
        try:
            self.client.goto(self.tab_screens[index])
        except ProtocolError as err:
            if err.reply.screen:
                self.set_output(err.reply.content)
            else:
                self.recover(str(err))

    def do_login(self):
        creds = self.check(lambda: (need_word(self.user_var.get(), "Username"),
                                    need_word(self.pass_var.get(), "Password")))
        if not creds:
            return
        ok, text = self.client.login(*creds)
        self.set_output(text)
        if ok:
            self.pass_var.set("")
            self.show_main(creds[0])
        else:
            messagebox.showerror("Login failed", text or "Invalid username or password.")

    def do_register(self):
        creds = self.check(lambda: (need_word(self.user_var.get(), "Username"),
                                    need_word(self.pass_var.get(), "Password")))
        if creds:
            self.run(self.client.register, *creds)

    def do_logout(self):
        self.run(self.client.logout)
        self.show_login()

    def existing_ids(self):
        """Read artifacts.txt ourselves ONLY to warn about duplicate IDs before typing them in."""
        path = os.path.join(self.client.s.workdir, "artifacts.txt")
        ids = set()
        try:
            with open(path, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    parts = line.rstrip("\n").split("|")
                    if len(parts) > 1:
                        ids.add(parts[1])
        except OSError:
            pass
        return ids

    def do_search_id(self):
        aid = self.check(lambda: need_text(self.search_id_var.get(), "Artifact ID"))
        if aid:
            self.run(self.client.search_id, aid)

    def do_add(self):
        kind = TYPES.index(self.type_var.get()) + 1

        def build():
            aid = need_text(self.f["id"].get(), "Artifact ID")
            if aid in self.existing_ids():
                raise ValueError("An artifact with ID '%s' already exists." % aid)
            data = dict(
                kind=kind, aid=aid,
                name=need_text(self.f["name"].get(), "Name"),
                age=need_int(self.f["age"].get(), "Estimated age", 0, 100000),
                cond=need_float(self.f["cond"].get(), "Condition score", 0.0, 10.0),
                loc=need_text(self.f["loc"].get(), "Current location"))
            if kind in INDEX_LABEL:
                data["extra"] = need_float(self.f["extra"].get(), INDEX_LABEL[kind].split(" (")[0], 0.0, 1.0)
            else:
                data["metal"] = self.metal_var.get()
                if kind == 3:
                    data["dynasty"] = need_text(self.f["dynasty"].get(), "Dynasty")
            return data

        data = self.check(build)
        if data and self.run(lambda: self.client.add_artifact(**data)) and "saved successfully" in self.output.get("1.0", "end"):
            for var in self.f.values():
                var.set("")

    def do_move(self):
        values = self.check(lambda: (need_text(self.mv["id"].get(), "Artifact ID"),
                                     need_text(self.mv["to"].get(), "New location"),
                                     need_text(self.mv["why"].get(), "Reason")))
        if values:
            self.run(self.client.move, *values)

    def do_history(self):
        aid = self.check(lambda: need_text(self.mv["hist"].get(), "Artifact ID"))
        if aid:
            self.run(self.client.history, aid)

    def do_undo(self):
        try:
            text, needs_confirm = self.client.undo_begin()
        except ProtocolError as err:
            self.recover(str(err))
            return
        self.set_output(text)
        if needs_confirm:
            yes = messagebox.askyesno("Undo last move", text + "\n\nUndo it?")
            self.run(self.client.undo_answer, yes)

    def do_queue(self):
        n = self.check(lambda: need_int(self.queue_var.get(), "Count", 1, 50))
        if n:
            self.run(self.client.restoration_queue, n)

    def do_find_hash(self):
        aid = self.check(lambda: need_text(self.hash_var.get(), "Artifact ID"))
        if aid:
            self.run(self.client.find_hash, aid)

    def do_filter_condition(self):
        def build():
            lo = need_float(self.lo_var.get(), "Minimum condition", 0.0, 10.0)
            hi = need_float(self.hi_var.get(), "Maximum condition", lo, 10.0)
            return lo, hi
        values = self.check(build)
        if values:
            self.run(self.client.filter_condition, *values)

    def on_close(self):
        self.client.close()
        self.destroy()


def main():
    exe = find_exe()
    if not exe:
        root = tk.Tk()
        root.withdraw()
        messagebox.showinfo("ArtiTrack", "Could not find artitrack.exe next to this script.\n"
                                         "Please select it (build it first with: mingw32-make).")
        exe = filedialog.askopenfilename(title="Select artitrack.exe",
                                         filetypes=[("Program", "*.exe"), ("All files", "*")])
        root.destroy()
        if not exe:
            sys.exit(1)
    App(exe).mainloop()


if __name__ == "__main__":
    main()