#!/bin/sh
# Test for Python Extension for PRoot
set -eu

# Check for test dependencies
for cmd in mcookie cat grep rm find; do
  if ! command -v "${cmd}" > /dev/null; then
      exit 125
  fi
done

# Check for PRoot binary
if [ ! -e "${PROOT}" ]; then
    exit 125
fi

# Check for python flag
if ! "${PROOT}" --help | grep -- -p | grep string; then
    exit 125
fi

TMP="$(mcookie)_hide.py"

# The following python script will hide all files
cat > "/tmp/${TMP}" <<EOF
import struct

from proot import *

class Dents(object):
	"""docstring for Dents"""
	def __init__(self):
		super(Dents, self).__init__()
		self.d_ino = 0
		self.d_off = 0
		self.d_reclen = 0
		self.d_name = None
		self.d_type = 0

	def get_real_size(self, buf):
		res = 0
		for b in buf:
			if b == 0:
				return res
			res += 1
		return res

	def unpack(self, buf):
		dirent_header_format = "LLH"
		(self.d_ino, self.d_off, self.d_reclen) = struct.unpack_from(dirent_header_format, buf)
		max_size = self.d_reclen - 2 - struct.calcsize(dirent_header_format)
		d_name = struct.unpack_from('%dB' % max_size, buf, struct.calcsize(dirent_header_format))
		real_size = self.get_real_size(d_name)
		(self.d_name, ) = struct.unpack_from('%ds' % real_size, buf, struct.calcsize(dirent_header_format))
		(self.d_type, ) = struct.unpack_from('B', buf, self.d_reclen - 1)

		return self.d_reclen

	def pack(self):
		dirent_header_size = struct.calcsize("LLH")
		d_name_length = len(self.d_name)
		d_reclen = d_name_length + 2 + dirent_header_size
		d_reclen = (d_reclen + 7) & ~7
		d_name_length = d_reclen - 2 - dirent_header_size
		buf = struct.pack('LLH%dsBB' % d_name_length, self.d_ino, self.d_off, self.d_reclen, self.d_name, 0, self.d_type)

		return buf

def get_getdents(tracee, res, addr):
	dents = []
	buf = read_data(tracee, addr, res)
	pos = 0
	while pos < res:
		d = Dents()
		pos += d.unpack(buf[pos:])
		dents.append(d)

	return dents

def put_dents(tracee, addr, dents):
	pos = 0
	for d in dents:
		buf = d.pack()
		write_data(tracee, pos + addr, buf)
		pos += len(buf)

	poke_reg(tracee, SYSARG_RESULT, pos)

def filter_dents(dents):
	res = []
	for d in dents:
		#we remove all files
		if d.d_type != 8:
			res.append(d)

	return res

def python_callback(extension, event, data1, data2):
	if event == SYSCALL_EXIT_END:
		tracee = get_tracee_from_extension(extension)
		no = get_sysnum(tracee, CURRENT)
		if no == PR_getdents:
			res = peek_reg(tracee, CURRENT, SYSARG_RESULT)
			if res > 0:
				addr = peek_reg(tracee, CURRENT, SYSARG_2)
				dents = get_getdents(tracee, res, addr)
				dents = filter_dents(dents)
				put_dents(tracee, addr, dents)

	return 0
EOF

# If script passes result from find will be empty
if [ "$(env PROOT_NO_SECCOMP=1 "${PROOT}" -p "/tmp/${TMP}" find . -maxdepth 1 -type f)" ]; then
    exit 1
fi

rm -rf "/tmp/${TMP}" "/tmp/${TMP}c"

