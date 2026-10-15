"""Low-priority, cancellable certification while the completed game is idle."""
from pathlib import Path
import threading,atexit,os
from tools.train_verified_lessons import train
class VerifiedTrainer:
    def __init__(self,memory,binary):
        self.memory=Path(memory);self.binary=Path(binary);self.worker=None;self.cancel=threading.Event();self.lock=threading.Lock();self.child=None;self.scheduled=False
        atexit.register(self.stop)
    def process(self,child):
        with self.lock:
            self.child=child
            if child:
                try:os.setpriority(os.PRIO_PROCESS,child.pid,10)
                except (OSError,AttributeError):pass
                if self.cancel.is_set():child.terminate()
    def stop(self):
        self.cancel.set()
        with self.lock:
            if self.child and self.child.poll() is None:self.child.terminate()
    def notify(self,result,command):
        if not self.binary.exists() or not result.get('ok'):return
        if command.startswith('new '):self.stop();self.scheduled=False;return
        if result.get('winner',-1)<0 or self.scheduled:return
        if self.worker and self.worker.is_alive():return
        self.scheduled=True;self.cancel=threading.Event()
        def run():
            try:train(self.memory,self.binary,self.cancel,self.process)
            except (OSError,ValueError,RuntimeError):pass
        self.worker=threading.Thread(target=run,daemon=True);self.worker.start()
