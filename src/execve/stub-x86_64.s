.text
.global _start
_start: // exit(182)
	movq    $182,%rdi
	movq    $60,%rax
	syscall
