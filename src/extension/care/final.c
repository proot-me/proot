/* -*- c-set-style: "K&R"; c-basic-offset: 8 -*-
 *
 * This file is part of PRoot.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <unistd.h>       /* lstat(2), readlink(2), getpid(2), wirte(2), lseek(2), get*id(2), */
#include <sys/types.h>    /* get*id(2), */
#include <sys/stat.h>     /* struct stat, fchmod(2), */
#include <linux/limits.h> /* PATH_MAX, */
#include <sys/utsname.h>  /* uname(2), */
#include <stdio.h>        /* fprintf(3), fclose(3), */
#include <errno.h>        /* errno, ENAMETOOLONG, */
#include <string.h>       /* strcpy(3), */
#include <endian.h>       /* htobe64(3), */
#include <assert.h>       /* assert(3), */

#include "extension/care/final.h"
#include "extension/care/care.h"
#include "extension/care/extract.h"
#include "execve/ldso.h"
#include "path/path.h"
#include "path/temp.h"
#include "cli/note.h"

/**
 * Find in @care->volatile_envars the given @envar (format
 * "name=value").  This function returns the name of the variable if
 * found (format "name"), NULL otherwise.
 */
static const char *find_volatile_envar(const Care *care, const char *envar)
{
	const Item *volatile_envar;

	if (care->volatile_envars == NULL)
		return NULL;

	STAILQ_FOREACH(volatile_envar, care->volatile_envars, link) {
		if (is_env_name(envar, volatile_envar->load))
			return volatile_envar->load;
	}

	return NULL;
}

extern char **environ;

/**
 * Archive in @care->archive the content of @file with the given
 * @name, then close it.  This function returns < 0 if an error
 * occured, otherwise 0.  Note: this function is called in @care's
 * destructor.
 */
static int archive_close_file(const Care *care, FILE *file, const char *name)
{
	char path[PATH_MAX];
	struct stat statl;
	char *location;
	int status;
	int fd;

	/* Ensure everything is written into the file before archiving
	 * it.  */
	fflush(file);

	fd = fileno(file);

	status = fstat(fd, &statl);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't get '%s' status", name);
		goto end;
	}

	location = talloc_asprintf(care, "%s/%s", care->prefix, name);
	if (location == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate location for '%s'", name);
		status = -1;
		goto end;
	}

	status = readlink_proc_pid_fd(getpid(), fd, path);
	if (status < 0) {
		note(NULL, ERROR, INTERNAL, "can't readlink(/proc/self/fd/%d)", fd);
		goto end;
	}

	status = archive(NULL, care->archive, path, location, &statl);
end:
	(void) fclose(file);
	return status;
}

/**
 * Return a copy -- attached to @context -- of @input with all '
 * (single quote) characters escaped.
 */
static const char *escape_quote(TALLOC_CTX *context, const char *input)
{
	char *output;
	size_t length;
	size_t i;

	output = talloc_strdup(context, "");
	if (output == NULL)
		return NULL;

	length = strlen(input);
	for (i = 0; i < length; i++) {
		char buffer[2] = { input[i], '\0' };

		if (buffer[0] == '\'')
			output = talloc_strdup_append_buffer(output, "'\\''");
		else
			output = talloc_strdup_append_buffer(output, buffer);
		if (output == NULL)
			return NULL;
	}

	return output;
}

/* Helpers for archive_* functions.  */
#define N(format, ...)							\
	do {								\
		if (fprintf(file, format "\n", ##__VA_ARGS__) < 0) {	\
			note(NULL, ERROR, INTERNAL, "can't write file"); \
			(void) fclose(file);				\
			return -1;					\
		}							\
	} while (0)

#define C(format, ...) N(format " \\", ##__VA_ARGS__)

/**
 * Archive the "re-execute.sh" file, according to the given @care.
 * This function returns < 0 if an error occured, 0 otherwise.  Note:
 * this function is called in @care's destructor.
 */
static int archive_re_execute_sh(Care *care)
{
	struct utsname utsname;
	const Item *item;
	FILE *file;
	int status;
	int i;

	file = open_temp_file(NULL, "care");
	if (file == NULL) {
		note(NULL, ERROR, INTERNAL, "can't create temporary file for 're-execute.sh'");
		return -1;
	}

	status = fchmod(fileno(file), 0755);
	if (status < 0)
		note(NULL, WARNING, SYSTEM, "can't make 're-execute.sh' executable");

	N("#! /bin/sh");
	N("");
	N("export XAUTHORITY=\"${XAUTHORITY:-$HOME/.Xauthority}\"");
	N("export ICEAUTHORITY=\"${ICEAUTHORITY:-$HOME/.ICEauthority}\"");
	N("");

	N("nbargs=$#");
	C("[ $nbargs -ne 0 ] || set --");
	for (i = 0; care->command != NULL && care->command[i] != NULL; i++)
		C("'%s'", care->command[i]);
	N("");

	N("PROOT=\"${PROOT-$(dirname $0)/proot}\"");
	N("");

	N("if [ ! -e ${PROOT} ]; then");
	N("    PROOT=$(which proot)");
	N("fi");
	N("");

	N("if [ -z ${PROOT} ]; then");
	N("    echo '**********************************************************************'");
	N("    echo '\"proot\" command not found, please get it from http://proot.me'");
	N("    echo '**********************************************************************'");
	N("    exit 1");
	N("fi");
	N("");

	N("if [ x$PROOT_NO_SECCOMP != x ]; then");
	N("    PROOT_NO_SECCOMP=\"PROOT_NO_SECCOMP=$PROOT_NO_SECCOMP\"");
	N("fi");
	N("");

	C("env --ignore-environment");
	C("PROOT_IGNORE_MISSING_BINDINGS=1");
	C("$PROOT_NO_SECCOMP");

	for (i = 0; environ[i] != NULL; i++) {
		const char *volatile_envar;

		volatile_envar = find_volatile_envar(care, environ[i]);
		if (volatile_envar != NULL)
			C("'%1$s'=\"$%1$s\" ", volatile_envar);
		else {
			const char *string = escape_quote(care, environ[i]);
			C("'%s' ", string ?: environ[i]);
		}
	}

	C("\"${PROOT-$(dirname $0)/proot}\"");

	if (care->volatile_paths != NULL) {
		/* If a volatile path is relative to $HOME, use an
		 * asymmetric binding.  For instance:
		 *
		 *     -b $HOME/.Xauthority:/home/user/.Xauthority
		 *
		 * where "/home/user" was the $HOME during the
		 * original execution.  */
		STAILQ_FOREACH(item, care->volatile_paths, link) {
			const char *name = talloc_get_name(item);
			if (name[0] == '$')
				C("-b \"%s:%s\" ", name, (char *) item->load);
			else
				C("-b \"%s\" ", (char *) item->load);
		}
	}

	status = uname(&utsname);
	if (status < 0) {
		note(NULL, WARNING, SYSTEM, "can't get kernel release");
		C("-k 3.17.0");
	}
	else {
		C("-k '\\%s\\%s\\%s\\%s\\%s\\%s\\0\\' ",
			utsname.sysname,
			utsname.nodename,
			utsname.release,
			utsname.version,
			utsname.machine,
			utsname.domainname);
	}

	C("-i %d:%d", getuid(), getgid());
	C("-w '%s' ", care->initial_cwd);
	C("-r \"$(dirname $0)/rootfs\"");

	/* In case the program retrieves its DSOs from /proc/self/maps
	 * (eg. VLC). */
	C("-b \"$(dirname $0)/rootfs\"");
	N("${1+\"$@\"}");
	N("");

	N("status=$?");
	N("if [ $status -ne %d ] && [ $nbargs -eq 0 ]; then", care->last_exit_status);
	N("echo \"care: The reproduced execution didn't return the same exit status as the\"");
	N("echo \"care: original execution.  If it is unexpected, please report this bug\"");
	N("echo \"care: to CARE/PRoot developers:\"");
	N("echo \"care:     * mailing list: reproducible@googlegroups.com; or\"");
	N("echo \"care:     * forum: https://groups.google.com/forum/?fromgroups#!forum/reproducible; or\"");
	N("echo \"care:     * issue tracker: https://github.com/cedric-vincent/PRoot/issues/\"");
	N("fi");
	N("");
	N("exit $status");

	return archive_close_file(care, file, "re-execute.sh");
}

/**
 * Archive the "concealed-accesses.txt" file in @care->archive,
 * according to the content of @care->concealed_accesses.  This
 * function returns < 0 if an error occured, 0 otherwise.  Note: this
 * function is called in @care's destructor.
 */
static int archive_concealed_accesses_txt(const Care *care)
{
	const Item *item;
	FILE *file;

	if (care->concealed_accesses == NULL)
		return 0;

	file = open_temp_file(NULL, "care");
	if (file == NULL) {
		note(NULL, WARNING, INTERNAL,
			"can't create temporary file for 'concealed-accesses.txt'");
		return -1;
	}

	STAILQ_FOREACH(item, care->concealed_accesses, link)
		N("%s", (char *) item->load);

	return archive_close_file(care, file, "concealed-accesses.txt");
}

/**
 * Archive the "README.txt" file in @care->archive.  This function
 * returns < 0 if an error occured, 0 otherwise.  Note: this function
 * is called in @care's destructor.
 */
static int archive_readme_txt(const Care *care)
{
	FILE *file;

	file = open_temp_file(NULL, "care");
	if (file == NULL) {
		note(NULL, WARNING, INTERNAL, "can't create temporary file for 'README.txt'");
		return -1;
	}

	N("This archive was created with CARE: https://proot-me.github.io. It contains:");
	N("");
	N("re-execute.sh");
	N("    start the re-execution of the initial command as originally");
	N("    specified.  It is also possible to specify an alternate command.");
	N("    For example, assuming gcc was archived, it can be re-invoked");
	N("    differently:");
	N("");
	N("        $ ./re-execute.sh gcc --version");
	N("        gcc (Ubuntu/Linaro 4.5.2-8ubuntu4) 4.5.2");
	N("");
	N("        $ echo 'int main(void) { return puts(\"OK\"); }' > rootfs/foo.c");
	N("        $ ./re-execute.sh gcc -Wall /foo.c");
	N("        $ foo.c: In function \"main\":");
	N("        $ foo.c:1:1: warning: implicit declaration of function \"puts\"");
	N("");
	N("rootfs/");
	N("    directory where all the files used during the original execution");
	N("    were archived, they will be required for the reproduced execution.");
	N("");
	N("proot");
	N("    virtualization tool invoked by re-execute.sh to confine the");
	N("    reproduced execution into the rootfs.  It also emulates the");
	N("    missing kernel features if needed.");
	N("");
	N("concealed-accesses.txt");
	N("    list of accessed paths that were concealed during the original");
	N("    execution.  Its main purpose is to know what are the paths that");
	N("    should be revealed if the the original execution didn't go as");
	N("    expected.  It is absolutely useless for the reproduced execution.");
	N("");

	return archive_close_file(care, file, "README.txt");
}

#undef N
#undef C

#if !defined(CARE_BINARY_IS_PORTABLE)
static int archive_myself(const Care *care) UNUSED;
#endif

/**
 * Archive the content pointed to by "/proc/self/exe" in
 * "@care->archive:@care->prefix/proot".  Note: this function is
 * called in @care's destructor.
 */
static int archive_myself(const Care *care)
{
	char path[PATH_MAX];
	struct stat statl;
	char *location;
	int status;

	status = readlink("/proc/self/exe", path, PATH_MAX);
	if (status >= PATH_MAX) {
		status = -1;
		errno = ENAMETOOLONG;
	}
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't readlink '/proc/self/exe'");
		return status;
	}
	path[status] = '\0';

	status = lstat(path, &statl);
	if (status < 0) {
		note(NULL, ERROR, INTERNAL, "can't lstat '%s'", path);
		return status;
	}

	location = talloc_asprintf(care, "%s/proot", care->prefix);
	if (location == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate location for 'proot'");
		return -1;
	}

	return archive(NULL, care->archive, path, location, &statl);
}

/**
 * Archive "re-execute.sh" & "proot" from @care.  This function
 * always returns 0.  Note: this is a Talloc destructor.
 */
int finalize_care(Care *care)
{
	char *extractor;
	int status;

	/* Generate & archive the "re-execute.sh" script. */
	status = archive_re_execute_sh(care);
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "can't archive 're-execute.sh'");

	/* Generate & archive the "concealed-accesses.txt" file. */
	status = archive_concealed_accesses_txt(care);
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "can't archive 'concealed-accesses.txt'");

	/* Generate & archive the "README.txt" file. */
	status = archive_readme_txt(care);
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "can't archive 'README.txt'");

#if defined(CARE_BINARY_IS_PORTABLE)
	/* Archive "care" as "proot", these are the same binary. */
	status = archive_myself(care);
	if (status < 0)
		note(NULL, WARNING, INTERNAL, "can't archive 'proot'");
#endif

	finalize_archive(care->archive);

	/* Append self/raw extracting information if needed.  */
	if (care->archive->fd >= 0 && care->archive->offset > 0) {
		AutoExtractInfo info;
		off_t position;

		strcpy(info.signature, AUTOEXTRACT_SIGNATURE);

		/* Compute the size of the archive.  */
		position = lseek(care->archive->fd, 0, SEEK_CUR);
		assert(position > care->archive->offset);
		info.size = htobe64(position - care->archive->offset);

		status = write(care->archive->fd, &info, sizeof(info));
		if (status != sizeof(info))
			note(NULL, WARNING, SYSTEM, "can't write extracting information");

		(void) close(care->archive->fd);
		care->archive->fd = -1;

		if (care->archive->offset == strlen("RAW"))
			extractor = talloc_asprintf(care, "`care -x %s`", care->output);
		else
			extractor = talloc_asprintf(care, "`%2$s%1$s` or `care -x %1$s`",
						care->output, care->output[0] == '/' ? "" : "./");
	}
	else if (care->output[strlen(care->output) - 1] != '/')
		extractor = talloc_asprintf(care, "`care -x %s`", care->output);
	else
		extractor = NULL;

	note(NULL, INFO, USER,
		"----------------------------------------------------------------------");
	note(NULL, INFO, USER, "Hints:");
	note(NULL, INFO, USER,
		"  - search for \"conceal\" in `care -h` if the execution didn't go as expected.");

	if (extractor != NULL)
		note(NULL, INFO, USER, "  - run %s to extract the output archive correctly.", extractor);

	return 0;
}
