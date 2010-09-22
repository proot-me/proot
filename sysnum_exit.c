switch (child->sysnum) {
case __NR_getcwd:
	result = get_sysarg(child->pid, SYSARG_RESULT);
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	status = detranslate_addr(child->pid, child->output, result, GETCWD);
	if (status < 0)
		break;

	set_sysarg(child->pid, SYSARG_RESULT, (word_t)status);
	break;

case __NR_readlink:
case __NR_readlinkat:
	result = get_sysarg(child->pid, SYSARG_RESULT);
	if ((int) result < 0) {
		status = 0;
		goto end;
	}

	/* Avoid the detranslation of partial result. */
	status = (int) get_sysarg(child->pid, SYSARG_3);
	if ((int) result == status) {
		status = 0;
		goto end;
	}

	assert((int) result <= status);

	status = detranslate_addr(child->pid, child->output, result, READLINK);
	if (status < 0)
		break;

	set_sysarg(child->pid, SYSARG_RESULT, (word_t)status);
	break;

default:
	status = 0;
	goto end;
}
