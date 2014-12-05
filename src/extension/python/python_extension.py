from proot import *

def python_callback(extension, event, data1, data2):
	#print tracee, event, data1, data2
	if event == 2:
		tracee = get_tracee_from_extension(extension)
		print "syscallcxv no from python : ", get_sysnum(tracee, ORIGINAL)
	return 0
