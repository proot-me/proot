.text
.global _start
_start: // exit(182)
	movl    $182,%ebx
	movl    $1,%eax
	int     $0x80
