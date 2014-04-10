=========
 Roadmap
=========

CARE v2.2
=========

* Based on PRoot v3.3.

* Use PRoot "fake id" option ("-i").

* Make "re-execute.sh" print a useful message when -h/--help is
  specified.

* Add an option to archive specified path, no matter whether it is
  used or not.

* In concealed-accesses.txt, print the name of the program that tried
  to access a concealed path too.

* Fake the full utsname structure.


CARE v2.3
=========

* Add an option to augment an existing archive with content used from
  a new monitored execution.


Not yet scheduled
=================

Fixes
-----

* Accept *care as valid CLI name

* Bug: an archive for "wine notepad.exe" created on Ubuntu 12.04
  x86_64 can't be re-executed on RHEL5 x86_64.

* Warn when an unemulated feature is used (ppoll, getcpu, flags, ...)
