#if defined(__i386__)
	.text

/*
	ABI	user-land  kernel-land
	======  =========  ===========
	number	%eax	   %eax
	arg1	%edx	   %ebx
	arg2	%ecx	   %ecx
	arg3	16(%esp)   %edx
	arg4	12(%esp)   %esi
	arg5	8(%esp)	   %edi
	arg6	4(%esp)    %ebp
	result	N/A	   %eax
*/
.globl	syscall_6
.type	syscall_6, @function
syscall_6:
	/* Callee-saved registers.  */
	pushl	%ebp	// %esp -= 0x04
	pushl	%edi	// %esp -= 0x08
	pushl	%esi	// %esp -= 0x0c
	pushl	%ebx	// %esp -= 0x10

//	mov	%eax, %eax		// number
	mov	%edx, %ebx		// arg1
//	mov	%ecx, %ecx		// arg2
	mov	0x14(%esp), %edx	// arg3
	mov	0x18(%esp), %esi	// arg4
	mov	0x1c(%esp), %edi	// arg5
	mov	0x20(%esp), %ebp	// arg6

	int	$0x80

	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp

//	mov	%eax, %eax		// result
	ret

.globl	syscall_3
.type	syscall_3, @function
syscall_3:
	pushl	%ebx
	mov	%edx, %ebx
	mov	0x8(%esp), %edx
	int	$0x80
	popl	%ebx
	ret

.globl	syscall_1
.type	syscall_1, @function
syscall_1:
	pushl	%ebx
	mov	%edx, %ebx
	int	$0x80
	popl	%ebx
	ret

#endif /* defined(__i386__) */
