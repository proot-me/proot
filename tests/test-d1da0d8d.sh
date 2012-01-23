${PROOT} -m /proc -m /tmp:/asym -w /asym ${ROOTFS} /bin/readlink /proc/self/cwd | grep '^/asym$'
${PROOT} -m /proc -m /tmp:/asym -w /tmp  ${ROOTFS} /bin/readlink /proc/self/cwd | grep '^/tmp$'
