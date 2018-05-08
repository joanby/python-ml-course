"""This module runs continuous updates for R, such as redrawing graphs when
the plot window is resized. Use the start() and stop() functions to turn
updates on and off.

"""
from rpy2.rinterface import process_revents
import time
import warnings
import threading


class _EventProcessorThread(threading.Thread):
    """ Call rinterface.process_revents(), pausing 
    for at least EventProcessor.interval between calls. """
    _continue = True

    def run(self):
        while self._continue:
            process_revents()
            time.sleep(EventProcessor.interval)
            
class EventProcessor(object):
    """ Processor for R events (Singleton class) """
    interval = 0.2
    daemon_thread = True
    name_thread = 'rpy2_process_revents'
    _thread = None
    _instance = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = object.__new__(cls)
        return cls._instance
    
    def start(self):
        """ start the event processor """
        if (self._thread is not None) and (self._thread.is_alive()):
            raise warnings.warn("Processing of R events already started.")
        else:
            self._thread = _EventProcessorThread(name = self.name_thread)
            self._thread.setDaemon(self.daemon_thread)
            self._thread.start()

    def stop(self):
        """ stop the event processor """
        self._thread._continue = False
        self._thread.join()
    
    thread = property(lambda self: self._thread, 
                      None, None, "Thread that processes the events.")

def start():
    """ Start the threaded processing of R events. """
    EventProcessor().start()

def stop():
    """ Stop the threaded processing of R events. """
    EventProcessor().stop()

