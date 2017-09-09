#include <sys/types.h>  /* stat(2), opendir(3), */
#include <sys/stat.h>   /* stat(2), chmod(2), */
#include <unistd.h>     /* stat(2), rmdir(2), unlink(2), readlink(2), */
#include <errno.h>      /* errno(2), */
#include <dirent.h>     /* readdir(3), opendir(3), */
#include <string.h>     /* strcmp(3), */
#include <stdlib.h>     /* free(3), */
#include <stdio.h>      /* P_tmpdir, */
#include <talloc.h>     /* talloc(3), */

#include "cli/note.h"

/**
 * Remove recursively the content of the current working directory.
 * This latter has to lie in P_tmpdir (ie. "/tmp" on most systems).
 * This function returns -1 if a fatal error occured (ie. the
 * recursion must be stopped), the number of non-fatal errors
 * otherwise.
 *
 * WARNING: this function changes the current working directory for
 * the calling process.
 */
static int clean_temp_cwd()
{
	const size_t length_tmpdir = strlen(P_tmpdir);
	char prefix[length_tmpdir];
	int nb_errors = 0;
	int status;
	DIR *dir;

	/* Sanity check: ensure the current directory lies in
	 * "/tmp".  */
	status = readlink("/proc/self/cwd", prefix, length_tmpdir);
	if (status < 0) {
		note(NULL, WARNING, SYSTEM, "can't readlink '/proc/self/cwd'");
		return ++nb_errors;
	}
	if (strncmp(prefix, P_tmpdir, length_tmpdir) != 0) {
		note(NULL, ERROR, INTERNAL,
			"trying to remove a directory outside of '%s', "
			"please report this error.\n", P_tmpdir);
		return ++nb_errors;
	}

	dir = opendir(".");
	if (dir == NULL) {
		note(NULL, WARNING, SYSTEM, "can't open '.'");
		return ++nb_errors;
	}

	while (1) {
		struct dirent *entry;

		errno = 0;
		entry = readdir(dir);
		if (entry == NULL)
			break;

		if (   strcmp(entry->d_name, ".")  == 0
		    || strcmp(entry->d_name, "..") == 0)
			continue;

		status = chmod(entry->d_name, 0700);
		if (status < 0) {
			note(NULL, WARNING, SYSTEM, "cant chmod '%s'", entry->d_name);
			nb_errors++;
			continue;
		}

		if (entry->d_type == DT_DIR) {
			status = chdir(entry->d_name);
			if (status < 0) {
				note(NULL, WARNING, SYSTEM, "can't chdir '%s'", entry->d_name);
				nb_errors++;
				continue;
			}

			/* Recurse.  */
			status = clean_temp_cwd();
			if (status < 0) {
				nb_errors = -1;
				goto end;
			}
			nb_errors += status;

			status = chdir("..");
			if (status < 0) {
				note(NULL, ERROR, SYSTEM, "can't chdir to '..'");
				nb_errors = -1;
				goto end;
			}

			status = rmdir(entry->d_name);
		}
		else {
			status = unlink(entry->d_name);
		}
		if (status < 0) {
			note(NULL, WARNING, SYSTEM, "can't remove '%s'", entry->d_name);
			nb_errors++;
			continue;
		}
	}
	if (errno != 0) {
		note(NULL, WARNING, SYSTEM, "can't readdir '.'");
		nb_errors++;
	}

end:
	(void) closedir(dir);
	return nb_errors;
}

/**
 * Remove recursively @path.  This latter has to be a directory lying
 * in P_tmpdir (ie. "/tmp" on most systems).  This function returns -1
 * on error, otherwise 0.
 */
static int remove_temp_directory2(const char *path)
{
	int result;
	int status;
	char *cwd;

	cwd = get_current_dir_name();

	status = chmod(path, 0700);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chmod '%s'", path);
		result = -1;
		goto end;
	}

	status = chdir(path);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chdir to '%s'", path);
		result = -1;
		goto end;
	}

	status = clean_temp_cwd();
	result = (status == 0 ? 0 : -1);

	/* Try to remove path even if something went wrong.  */
	status = chdir("..");
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "can't chdir to '..'");
		result = -1;
		goto end;
	}

	status = rmdir(path);
	if (status < 0) {
		note(NULL, ERROR, SYSTEM, "cant remove '%s'", path);
		result = -1;
		goto end;
	}

end:
	if (cwd != NULL) {
		status = chdir(cwd);
		if (status < 0) {
			result = -1;
			note(NULL, ERROR, SYSTEM, "can't chdir to '%s'", cwd);
		}
		free(cwd);
	}

	return result;
}

/**
 * Like remove_temp_directory2() but always return 0.
 *
 * Note: this is a talloc destructor.
 */
static int remove_temp_directory(char *path)
{
	(void) remove_temp_directory2(path);
	return 0;
}

/**
 * Remove the file @path.  This function always returns 0.
 *
 * Note: this is a talloc destructor.
 */
static int remove_temp_file(char *path)
{
	int status;

	status = unlink(path);
	if (status < 0)
		note(NULL, ERROR, SYSTEM, "can't remove '%s'", path);

	return 0;
}

/**
 * Create a path name with the following format:
 * "/tmp/@prefix-$PID-XXXXXX".  The returned C string is either
 * auto-freed if @context is NULL.  This function returns NULL if an
 * error occurred.
 */
char *create_temp_name(TALLOC_CTX *context, const char *prefix)
{
	char *name;

	if (context == NULL)
		context = talloc_autofree_context();

	name = talloc_asprintf(context, "%s/%s-%d-XXXXXX", P_tmpdir, prefix, getpid());
	if (name == NULL) {
		note(NULL, ERROR, INTERNAL, "can't allocate memory");
		return NULL;
	}

	return name;
}

/**
 * Create a directory that will be automatically removed either on
 * PRoot termination if @context is NULL, or once its path name
 * (attached to @context) is freed.  This function returns NULL on
 * error, otherwise the absolute path name to the created directory
 * (@prefix-ed).
 */
const char *create_temp_directory(TALLOC_CTX *context, const char *prefix)
{
	char *name;

	name = create_temp_name(context, prefix);
	if (name == NULL)
		return NULL;

	name = mkdtemp(name);
	if (name == NULL) {
		note(NULL, ERROR, SYSTEM, "can't create temporary directory");
		return NULL;
	}

	talloc_set_destructor(name, remove_temp_directory);

	return name;
}

/**
 * Create a file that will be automatically removed either on PRoot
 * termination if @context is NULL, or once its path name (attached to
 * @context) is freed.  This function returns NULL on error,
 * otherwise the absolute path name to the created file (@prefix-ed).
 */
const char *create_temp_file(TALLOC_CTX *context, const char *prefix)
{
	char *name;
	int fd;

	name = create_temp_name(context, prefix);
	if (name == NULL)
		return NULL;

	fd = mkstemp(name);
	if (fd < 0) {
		note(NULL, ERROR, SYSTEM, "can't create temporary file");
		return NULL;
	}
	close(fd);

	talloc_set_destructor(name, remove_temp_file);

	return name;
}

/**
 * Like create_temp_file() but returns an open file stream to the
 * created file.  It's up to the caller to close returned stream.
 */
FILE* open_temp_file(TALLOC_CTX *context, const char *prefix)
{
	char *name;
	FILE *file;
	int fd;

	name = create_temp_name(context, prefix);
	if (name == NULL)
		return NULL;

	fd = mkstemp(name);
	if (fd < 0)
		goto error;

	talloc_set_destructor(name, remove_temp_file);

	file = fdopen(fd, "w");
	if (file == NULL)
		goto error;

	return file;

error:
	if (fd >= 0)
		close(fd);
	note(NULL, ERROR, SYSTEM, "can't create temporary file");
	return NULL;
}
