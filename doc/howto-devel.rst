Setup a development environment for PRoot
=================================================

This document provides instructions for preparing
a system for developing PRoot and CARE.

Docker
------

The following command will attempt to build an image for
each supported distribution::

  make -C test check-test-docker.sh V=1

Vagrant
-------

Fork this github repostiory located at `<https://github.com/proot-me/proot>`_

``git clone [YOUR_REPOSITORY_URL]`` Clone the repository on your local machine

``sudo apt-get install vagrant vagrant-sshfs`` Install vagrant ( For Debian and derivatives )

.. _vagrant-sshfs: https://github.com/dustymabe/vagrant-sshfs

``cd proot`` change directory into proot repository. 
 
Initialize a virtual machine for each supported distribution::

  for distro in alpine centos debian; do
    cd "test/vagrant/${distro}"
    vagrant up
    cd ../../../
  done

You should now see 3 virtual machines up and running in your vitualbox manager interface. :) 


To access the virtual machines cd in the vagrant location of one of the supported distribution
 - ``cd "test/vagrant/debian``
 - ``cd "test/vagrant/alpine``
 - ``cd "test/vagrant/centos``


And ssh into the machine with ``vagrant ssh``

Now you can follow the compiling instructions located in the README.rst file. :) 
