#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

void cleanup(void)
{
  chmod("top-bug-138", 0777);
  rmdir("top-bug-138/a/b");
  rmdir("top-bug-138/a");
  rmdir("top-bug-138");
}

int main()
{
  struct stat statbuf;
  int i = atexit(cleanup);
  if (i != 0) {
    fprintf(stderr, "cannot set atexit cleanup\n");
    exit(EXIT_FAILURE);
  }
  
  int mydirtop=mkdir("top-bug-138", 0777);
  if (mydirtop != 0) {
    perror("mkdir top-bug-138");
    exit(EXIT_FAILURE);
  }

  int mydira=mkdir("top-bug-138/a", 0777);
  if (mydira != 0) {
    perror("mkdir top-bug-138/a");
    exit(EXIT_FAILURE);
  }

  int mydirb=mkdir("top-bug-138/a/b", 0777);
  if (mydirb != 0) {
    perror("mkdir top-bug-138/a/b");
    exit(EXIT_FAILURE);
  }

  int myunlinka1=unlink("top-bug-138/a");
  if (myunlinka1 == 0) {
    perror("unlink top-bug-138/a should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EISDIR) {
    perror("unlink top-bug-138/a should have failed with EISDIR");
    exit(EXIT_FAILURE);
  }

  int myrmdira=rmdir("top-bug-138/a");
  if (myrmdira == 0) {
    perror("rmdir top-bug-138/a should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != ENOTEMPTY) {
    perror("rmdir top-bug-138/a should have failed with ENOTEMPTY");
    exit(EXIT_FAILURE);
  }

  int mychmod1=chmod("top-bug-138", 0);
  if (mychmod1 != 0) {
    perror("chmod top-bug-138");
    exit(EXIT_FAILURE);
  }

  int myunlinkab=unlink("top-bug-138/a/b");
  if (myunlinkab == 0) {
    perror("unlink top-bug-138/a/b should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EACCES) {
    perror("unlink top-bug-138/a/b should have failed with EACCES");
    exit(EXIT_FAILURE);
  }

  int myrmdirab=rmdir("top-bug-138/a/b");
  if (myrmdirab == 0) {
    perror("rmdir top-bug-138/a/b should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EACCES) {
    perror("rmdir top-bug-138/a/b should have failed with EACCES");
    exit(EXIT_FAILURE);
  }

  int mystat=stat("top-bug-138/a/b", &statbuf);
  if (mystat == 0) {
    perror("stat top-bug-138/a/b should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EACCES) {
    perror("stat top-bug-138/a/b should have failed with EACCES");
    exit(EXIT_FAILURE);
  }

  int mychmod2=chmod("top-bug-138", 0700);
  if (mychmod2 != 0) {
    perror("chmod top-bug-138");
    exit(EXIT_FAILURE);
  }

  int myunlinka=unlink("top-bug-138/a");
  if (myunlinka == 0) {
    perror("unlink top-bug-138/a should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EISDIR) {
    perror("unlink top-bug-138/a should have failed with EISDIR");
    exit(EXIT_FAILURE);
  }

  myrmdira=rmdir("top-bug-138/a");
  if (myrmdira == 0) {
    perror("rmdir top-bug-138/a should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != ENOTEMPTY) {
    perror("unlink top-bug-138/a should have failed with ENOTEMPTY");
    exit(EXIT_FAILURE);
  }

  int myunlinktop=unlink("top-bug-138");
  if (myunlinktop == 0) {
    perror("unlink top-bug-138 should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != EISDIR) {
    perror("unlink top-bug-138 should have failed with EISDIR");
    exit(EXIT_FAILURE);
  }

  int myrmdirtop=rmdir("top-bug-138");
  if (myrmdirtop == 0) {
    perror("rmdir top-bug-138 should have failed");
    exit(EXIT_FAILURE);
  }
  if (errno != ENOTEMPTY) {
    perror("rmdir top-bug-138 should have failed with ENOTEMPTY");
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
