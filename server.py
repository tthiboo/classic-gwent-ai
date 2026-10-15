#!/usr/bin/env python3
"""Local, single-match tabletop. No dependencies or external services."""
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse
import json, subprocess, threading, argparse, os, atexit, shutil
ROOT = Path(__file__).resolve().parent
# Separate migrated memory from engines serving older, unfinished games.
MEMORY = ROOT/'bot-memory-v4.txt'
if not MEMORY.exists():
    for previous_memory in (ROOT/'bot-memory-v3.txt', ROOT/'bot-memory.txt'):
        if previous_memory.exists():
            shutil.copy2(previous_memory, MEMORY)
            # The learning priors/verified lessons belong to this memory too.
            # Migrating only the main profile would silently discard them.
            for suffix in ('.priors-v12', '.verified-v1'):
                source = Path(str(previous_memory)+suffix)
                target = Path(str(MEMORY)+suffix)
                if source.exists() and not target.exists():
                    shutil.copy2(source, target)
            break
from tools.verified_trainer import VerifiedTrainer
trainer = VerifiedTrainer(MEMORY, ROOT/'engine/verified_reviewer')
lock = threading.Lock()
proc = None
proc_binary = None

def binary_signature():
    info = (ROOT/'engine/classicbot').stat()
    return (info.st_ino, info.st_size, info.st_mtime_ns)

def engine(command):
    global proc, proc_binary
    with lock:
        # A new game is already an explicit reset. Activate an updated binary
        # here, without ever replacing the engine during an unfinished match.
        if command.startswith('new ') and proc is not None and proc.poll() is None and proc_binary != binary_signature():
            proc.terminate()
            try: proc.wait(timeout=3)
            except subprocess.TimeoutExpired: proc.kill(); proc.wait()
            proc = None
        if proc is None or proc.poll() is not None:
            proc = subprocess.Popen([str(ROOT/'engine/classicbot'), str(ROOT/'engine/decks.txt'), 'play', str(MEMORY)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
            proc_binary = binary_signature()
        proc.stdin.write(command+'\n'); proc.stdin.flush()
        line = proc.stdout.readline()
        if not line: raise RuntimeError('Игровой движок остановился. Начните новую партию.')
        result=json.loads(line)
        trainer.notify(result, command)
        return result

class Handler(SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs): super().__init__(*args,directory=str(ROOT/'web'),**kwargs)
    def log_message(self,*args): pass
    def end_headers(self):
        self.send_header('Cache-Control','private, max-age=604800' if self.path.startswith(('/assets/cards/','/assets/thumbs/','/assets/screen/')) else 'no-store')
        self.send_header('X-Content-Type-Options','nosniff')
        super().end_headers()
    def do_POST(self):
        origin=self.headers.get('Origin')
        if origin and origin != f'http://{self.headers.get("Host")}':
            self.send_error(403); return
        if self.path!='/api': self.send_error(404); return
        try:
            length=int(self.headers.get('Content-Length','0'))
            if not 0<length<8192: raise ValueError('Invalid request')
            data=json.loads(self.rfile.read(length))
            cmd=data.get('command','')
            if not isinstance(cmd,str) or '\n' in cmd or '\r' in cmd or len(cmd)>4096: raise ValueError('Invalid command')
            result=engine(cmd)
        except Exception as e: result={'ok':False,'error':str(e)}
        payload=json.dumps(result,ensure_ascii=False).encode()
        self.send_response(200); self.send_header('Content-Type','application/json; charset=utf-8'); self.send_header('Content-Length',str(len(payload))); self.end_headers();self.wfile.write(payload)

def cleanup():
    if proc and proc.poll() is None: proc.terminate()
atexit.register(cleanup)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--port',type=int,default=8765);args=parser.parse_args()
    binary=ROOT/'engine/classicbot'
    if not binary.exists(): subprocess.run(['c++','-std=c++17','-O2','-pthread',str(ROOT/'engine/classic_bridge.cpp'),'-o',str(binary)],check=True)
    server=ThreadingHTTPServer(('127.0.0.1',args.port),Handler)
    print(f'Гвинт: http://127.0.0.1:{server.server_port}',flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close();cleanup()
