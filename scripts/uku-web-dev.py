#!/usr/bin/env python3
import argparse
import functools
import json
import os
import subprocess
import sys
import threading
import time
import webbrowser
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DIST_DIR = os.path.join(ROOT, "build", "dist", "web")
WATCH_DIRS = [
    os.path.join(ROOT, "src"),
    os.path.join(ROOT, "locales"),
    os.path.join(ROOT, "assets"),
    os.path.join(ROOT, "..", "flint", "src"),
    os.path.join(ROOT, "..", "flint", "include"),
    os.path.join(ROOT, "..", "flint", "mk"),
]
WATCH_FILES = [
    os.path.join(ROOT, "Makefile"),
    os.path.join(ROOT, "flint.toml"),
]
WATCH_SUFFIXES = {
    ".c", ".h", ".html", ".mk", ".txt", ".png", ".dat", ".toml", ".json",
}


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.version = 0
        self.ok = False
        self.building = False

    def snapshot(self):
        with self.lock:
            return {
                "version": self.version,
                "ok": self.ok,
                "building": self.building,
            }

    def begin_build(self):
        with self.lock:
            self.building = True

    def finish_build(self, ok):
        with self.lock:
            self.ok = ok
            self.building = False
            if ok:
                self.version += 1


class Handler(SimpleHTTPRequestHandler):
    state = None

    def do_GET(self):
        if self.path.split("?", 1)[0] == "/__uku_reload":
            body = json.dumps(self.state.snapshot()).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        super().do_GET()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def latest_mtime():
    latest = 0.0
    for path in WATCH_FILES:
        try:
            latest = max(latest, os.path.getmtime(path))
        except OSError:
            pass
    for base in WATCH_DIRS:
        if not os.path.isdir(base):
            continue
        for current, dirs, files in os.walk(base):
            dirs[:] = [d for d in dirs if d not in {".git", "build", "dist", "work"}]
            for name in files:
                if os.path.splitext(name)[1] not in WATCH_SUFFIXES:
                    continue
                path = os.path.join(current, name)
                try:
                    latest = max(latest, os.path.getmtime(path))
                except OSError:
                    pass
    return latest


def build(state):
    state.begin_build()
    print("Building Uku web...")
    result = subprocess.run(["make", "web"], cwd=ROOT)
    ok = result.returncode == 0
    state.finish_build(ok)
    print("Build {}.".format("finished" if ok else "failed"))
    return ok


def watch(state, interval):
    stamp = latest_mtime()
    build(state)
    while True:
        time.sleep(interval)
        current = latest_mtime()
        if current > stamp:
            stamp = current
            build(state)


def open_browser(browser, url):
    if not browser:
        return
    try:
        subprocess.Popen([browser, url], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError:
        webbrowser.open(url)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=8080, type=int)
    parser.add_argument("--browser", default="brave-browser")
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--interval", default=1.0, type=float)
    args = parser.parse_args()

    os.makedirs(DIST_DIR, exist_ok=True)
    state = State()
    watcher = threading.Thread(target=watch, args=(state, args.interval), daemon=True)
    watcher.start()

    handler = functools.partial(Handler, directory=DIST_DIR)
    Handler.state = state
    server = ThreadingHTTPServer((args.host, args.port), handler)
    url = "http://{}:{}/".format(args.host, args.port)
    print("Serving Uku web at {}".format(url))
    if not args.no_browser:
        open_browser(args.browser, url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("")
        return 0


if __name__ == "__main__":
    sys.exit(main())
