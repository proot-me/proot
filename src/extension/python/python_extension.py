from proot import *
import ctypes
import imp

client = None

def python_callback(extension, event, data1, data2):
	global client
	res = 0

	if event == 11:
		if client:
			print "Already have a client => refuse to use %s" % (ctypes.string_at(data1))
		else:
			client = imp.load_source('client', ctypes.string_at(data1))
	if client:
		return client.python_callback(extension, event, data1, data2)

	return 0
