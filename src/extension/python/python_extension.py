from proot import *
import ctypes
import importlib.util
import sys

client = None

def python_callback(extension, event, data1, data2):
    global client
    res = 0
    if event == 11:
        if client:
            print("Already have a client => refuse to use %s" % (ctypes.string_at(data1).decode('utf-8')))
        else:
            module_path = ctypes.string_at(data1).decode('utf-8')
            
            spec = importlib.util.spec_from_file_location("client", module_path)
            client = importlib.util.module_from_spec(spec)
            sys.modules["client"] = client
            spec.loader.exec_module(client)
    
    if client:
        return client.python_callback(extension, event, data1, data2)
    return 0
