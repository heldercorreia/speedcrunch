.. highlight:: shell

Installation
============

Microsoft Windows
-----------------

SpeedCrunch for Windows requires Windows Vista or newer (32-bit and 64-bit versions).
You can use choose to use either the installer or the portable
version of SpeedCrunch. Both of these are available from `the SpeedCrunch website <sc_>`_.

.. _sc: http://speedcrunch.org

The installer installs SpeedCrunch on your system and creates an entry in the menu
to launch it. This process requires administrator rights.

Alternatively, you may use the portable version of SpeedCrunch, which is distributed
as a ``.zip`` archive. This version requires no installation, simply extract it to
a folder of your choice and run :program:`speedcrunch.exe`. In addition, this version of
the software is fully portable: It stores all its settings and user data next to the
executable so the entire directory can be copied to another computer and run
there.


macOS
-----

Download the macOS disk image from `the SpeedCrunch website <sc_>`_, open it, and
drag :guilabel:`SpeedCrunch` to the :guilabel:`Applications` folder.

On first launch, macOS may block SpeedCrunch because it cannot verify the developer.
Only continue if you downloaded SpeedCrunch from the official website and trust the
download. Try to open SpeedCrunch once, then:

#. Open the Apple menu and choose :guilabel:`System Settings` > :guilabel:`Privacy & Security`.
#. Scroll to :guilabel:`Security` and click :guilabel:`Open` or :guilabel:`Open Anyway`
   next to SpeedCrunch.
#. Confirm that you want to open the app. Enter your Mac password if asked.

The :guilabel:`Open Anyway` option is available for about an hour after your first
attempt. macOS remembers the exception, so you can open SpeedCrunch normally afterward.
See `Apple's step-by-step instructions <https://support.apple.com/en-am/guide/mac-help/mh40616/27/mac/27>`_.


Linux
-----

On Linux, SpeedCrunch can be installed either from packages provided by your distribution
or from official Flatpak packages.

Distribution Packages
+++++++++++++++++++++

Many Linux distributions already include packages for SpeedCrunch in their repositories. The
exact steps to install these depend on the distribution. For example, on Debian and
Ubuntu, the included SpeedCrunch package can be installed by running this command::

   sudo apt-get install speedcrunch

On Fedora, this command will install SpeedCrunch::

   sudo dnf install speedcrunch

Check your system's documentation to find out how to install additional packages.
Many desktop distributions also include graphical package management software that
can be used to install SpeedCrunch.


Flatpak
+++++++

`Flatpak <flatpak_>`_ is a distribution-independent packaging format for Linux desktop applications. The latest stable
release of SpeedCrunch is available as a Flatpak package from `Flathub <flathub_>`_.

First, visit Flathub's `setup page <flathub-setup_>`_ and follow the instructions for your distribution to set up
Flatpak and Flathub. Then install SpeedCrunch from its `store page <flathub-sc_>`_ on Flathub.

.. _flatpak: https://flatpak.org/
.. _flathub: https://flathub.org/
.. _flathub-setup: https://flatpak.org/setup/
.. _flathub-sc: https://flathub.org/apps/details/org.speedcrunch.SpeedCrunch


Building from Source
--------------------

The SpeedCrunch source code is maintained on `GitHub`_ in a Git repository. Clone
the repository onto your machine by running the following command::

    git clone https://github.com/heldercorreia/speedcrunch.git

.. _GitHub: https://github.com/heldercorreia/speedcrunch

The main readme file (:file:`README.md` at the root of the repository) contains
instructions on how to compile SpeedCrunch.
