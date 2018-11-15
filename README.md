[![CircleCI](https://circleci.com/gh/midori-browser/core.svg?style=svg)](https://circleci.com/gh/midori-browser/core)
[![Snap Status](https://build.snapcraft.io/badge/midori-browser/core.svg)](https://build.snapcraft.io/user/midori-browser/core)
[![Telegram](https://img.shields.io/badge/Telegram-Chat-gray.svg?style=flat&logo=telegram&colorA=5583a4&logoColor=fff)](https://www.midori-browser.org/telegram)
[![Twitter](https://img.shields.io/twitter/follow/midoriweb.svg?style=social&label=Follow)](https://twitter.com/midoriweb)
[![Donate](https://img.shields.io/badge/PayPal-Donate-gray.svg?style=flat&logo=paypal&colorA=0071bb&logoColor=fff)](https://www.midori-browser.org/donate)

<p align="center">
    <img src="icons/scalable/apps/midori.svg"/>
</p>

<p align="center">
    <b>Midori</b>
    a lightweight, fast and free web browser
</p>

![Midori Screenshot](https://www.midori-browser.org/images/screenshots/rdio.png)

Midori is a lightweight yet powerful web browser which runs just as well on little embedded computers named for delicious pastries as it does on beefy machines with a core temperature exceeding that of planet earth. And it looks good doing that, too. Oh, and of course it's free software.

**Privacy out of the box**

* Adblock filter list support.
* Private browsing.
* Manage cookies and scripts.

**Productivity features**

* Open a 1000 tabs instantly.
* Easy web apps creation.
* Customizable side panels.
* User scripts and styles a la Greasemonkey.
* Web developer tools powered by WebKit.

Please report comments, suggestions and bugs to:
    https://github.com/midori-browser/core/issues

Join [the #midori IRC channel](https://www.midori-browser.org/irc) on Freenode
or [the Telegram group](https://www.midori-browser.org/telegram)!

# Installing Midori on Linux

If [your distro supports snaps](https://docs.snapcraft.io/core/)
you can install the **latest stable** version of Midori
[from the snap store](https://snapcraft.io/midori) with a single command:

    snap install midori

> **Spoilers:** For those more adventurous types out there, trying out the preview of the next version is only the switch of a channel away.

# Building from source

**Requirements**

* GLib 2.48.0
* GTK+ 3.12
* gcr-3 2.32
* [WebKit2GTK+](https://webkitgtk.org/) 2.16.6
* libsoup 2.48.0
* sqlite 3.6.19
* [Vala](https://wiki.gnome.org/Projects/Vala) 0.30
* GCR 2.32
* Peas

Install dependencies on Ubuntu or Debian based distros:

    sudo apt install valac libwebkit2gtk-4.0-dev libsoup-gnome2.4-dev libgcr-3-dev libpeas-dev libsqlite3-dev intltool libxml2-utils

Use CMake to build Midori:

    mkdir _build
    cd _build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make
    sudo make install

> **Spoilers:** Pass `-G Ninja` to CMake to use [Ninja](http://martine.github.io/ninja) instead of make (install `ninja-build` on Ubuntu/ Debian).

Midori can be **run without being installed**.

    _build/midori

# Testing

## Unit tests

You'll want to **unit test** the code if you're testing a new version or contributed your own changes:

    xvfb-run make check

## Manual checklist

* Browser window starts up normally, with optional URL(s) on the command line
* Tabs have icons, a close button if there's more than one and can be switched
* Urlbar suggests from typed search or URL, completes from history and highlights key
* Private data can be cleared
* Shortcuts window shows most important hotkeys
* Download button lists on-going and finished downloads
* `javascript:alert("test")`, `javascript:confirm("test")` and `javascript:input("test")` work
* Websites can (un)toggle fullscreen mode
* Shrinking the window moves browser and page actions into the respective menus

# Release process

Update `CORE_VERSION` in `CMakeLists.txt` to `6.0`.
Add a section to `ChangeLog`.

    git commit -p -v -m "Release Midori 6.0"
    git checkout -B release-6.0
    git push origin HEAD
    git archive -o midori-v6.0.tar.gz -9 HEAD

Propose a PR for the release.
Publish the release on https://github.com/midori-browser/core/releases
Promote snap on https://snapcraft.io/midori/release to the `stable` channel

# Troubleshooting

Testing an installed release may reveal crashers or memory corruption which require investigating from a local build and obtaining a stacktrace (backtrace, crash log).

    gdb _build/midori
    run
    …
    bt

If the problem is a warning, not a crash GLib has a handy feature

    env G_MESSAGES_DEBUG=all gdb _build/midori

On Windows you can open the folder where Midori is installed and double-click gdb.exe which opens a command window:

    file midori.exe
    run
    …
    bt

To verify a regression you might need to revert a particular change:

    # Revert only d54c7e45
    git revert d54c7e45

# Contributing code

## Coding style and quality

Midori code should in general have:

  * 4 space indentation, no tabs
  * Between 80 to 120 columns
  * Use `//` or `/* */` style comments
  * Call variables `animal` and `animal_shelter` instead of ~camelCase~
  * Keep a space between functions/ keywords and round parentheses
  * Prefer `new Gtk.Widget ()` over `using Gtk; new Widget ()`
  * `Midori` and `GLib` namespaces should be omitted
  * Don't use `private` specifiers (which is the default)
  * Stick to standard Vala-style curly parentheses on the same line
  * Cuddled `} else {` and `} catch (Error error) {`

## Working with Git

If you haven't yet, [check that GitHub has your SSH key](https://github.com/settings/keys).
> **Spoilers:** You can create an SSH key with **Passwords and Keys** aka **Seahorse**
> or `ssh-keygen -t rsa` and specify `Host github.com` with `User git` in your SSH config.
> See [GitHub docs](https://help.github.com/articles/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent/) for further details.

[Fork the project on GitHub](https://help.github.com/articles/fork-a-repo).

    # USERNAME is your GitHub username
    git clone git@github.com:USERNAME/core.git

Prepare to pull in updates from upstream:

    git remote add upstream https://github.com/midori-browser/core.git

> **Spoilers:** The code used to be hosted at `lp:midori` and `git.xfce.org/apps/midori` respectively.

The development **master** (trunk, tip) is the latest iteration of the next release.

    git checkout upstream/master

Pick a name for your feature branch:

    git checkout -B myfeature

Remember to keep your branch updated:

    git pull -r upstream master

Tell git your name if you haven't yet:

    git config user.email "<email@address>"
    git config user.name "Real Name"

See what you did so far

    git diff

Get an overview of changed and new files:

    git status -u

Add new files, move/ rename or delete:

    git add FILENAME
    mv OLDFILE NEWFILE
    rm FILENAME

Commit all current changes, selected interactively:

    git commit -p -v

If you have one or more related bug reports you should mention them
in the commit message. Once these commits are merged the bug will
automatically be closed and the commit log shows clickable links to the reports:

    Fixes: #123

If you've made several commits:

    git log

In the case you committed something wrong or want to amend it:

    git reset --soft HEAD^

If you end up with unrelated debugging code or other patches in the current changes
it's sometimes handy to temporarily clean up.
This may be seen as git's version of `bzr shelve`:

    git stash save
    git commit -p -v
    git stash apply

As a general rule of thumb, `git COMMAND --help` gives you an explanation
of any command and `git --help -a` lists all available commands.

Push your branch and **propose it for merging into master**.

    git push origin HEAD

This will automatically request a **review from other developers** who can then comment on it and provide feedback.

# Jargon

* **freeze**: a period of bug fixes eg. 4/2 cycle means 4 weeks of features and 2 weeks of stabilization
* **PR**: pull request, a branch proposed for review, analogous to **MR** (merge request) with Bazaar
* **ninja**: an internal tab, usually empty label, used for taking screenshots
* **fortress**: user of an ancient release like 0.4.3 as found on Raspberry Pie, Debian, Ubuntu
* **katze, sokoke, tabby**: legacy API names and coincidentally cat breeds

# Midori for Windows

## For Linux developers

### Dependencies

Midori for Windows is compiled on a Linux host and MinGW stack. For the current build Fedora 18 packages are used. Packages needed are listed below:

    yum install gcc vala intltool

For a native build

    yum install libsoup-devel webkitgtk3-devel sqlite-devel

For cross-compilation

    yum install mingw{32,64}-webkitgtk3 mingw{32,64}-glib-networking mingw{32,64}-gdb mingw{32,64}-gstreamer-plugins-good

Packages needed when assembling the archive

    yum install faenza-icon-theme p7zip mingw32-nsis greybird-gtk3-theme

Installing those should get you the packages needed to successfully build and develop Midori for Win32.

### Building

For 32-bit builds:

    mkdir _mingw32
    cd _mingw32
    mingw32-cmake .. -DCMAKE_INSTALL_PREFIX=/usr/i686-w64-mingw32/sys-root/mingw -DCMAKE_VERBOSE_MAKEFILE=0
    make
    sudo make install

For 64-bit builds:

    mkdir _mingw64
    cd _mingw64
    mingw64-cmake .. -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32/sys-root/mingw -DCMAKE_VERBOSE_MAKEFILE=0
    make
    sudo make install

Once built and tested you can assemble the Midori archive with a helper script

32-bit build:

    env MINGW_PREFIX="/usr/i686-w64-mingw32/sys-root/mingw" ./win32/makedist/makedist.midori

64-bit build:

    env MINGW_PREFIX="/usr/x86_64-w64-mingw32/sys-root/mingw/" ./win32/makedist/makedist.midori x64

### Testing

For testing your changes a real system is recommended because WebKitGTK+ doesn't work properly under Wine. Mounting your MinGW directories as a network drive or shared folder in a Windows VM is a good option.

## For Windows developers

### Prerequisites

* [MinGW](http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/rubenvb/gcc-4.8-release/x86_64-w64-mingw32-gcc-4.8.0-win32_rubenvb.7z/download) *mingw64 rubenvb*/ gcc 4.8.0 ([Releases](http://sourceforge.net/projects/mingw-w64/files/Toolchains%20targetting%20Win64/Personal%20Builds/rubenvb/gcc-4.8-release))
* [7zip](http://www.7-zip.org/download.html) ([32bit Installer](http://downloads.sourceforge.net/sevenzip/7z920.exe)) to extract archives
* [Python3](http://www.python.org/download/releases/3.3.5) to use **download-mingw-rpm.py**.
* [download-mingw-rpm.py](https://github.com/mkbosmans/download-mingw-rpm/blob/master/download-mingw-rpm.py) to fetch and unpack rpm's
* [Msys](http://sourceforge.net/projects/mingw-w64/files/External%20binary%20packages%20%28Win64%20hosted%29/MSYS%20%2832-bit%29/MSYS-20111123.zip/download) contains shell and some small utilities
* [CMake](http://www.cmake.org/cmake/resources/software.html) ([Installer](http://www.cmake.org/files/v2.8/cmake-2.8.12.2-win32-x86.exe))
* [Vala](http://ftp.gnome.org/pub/gnome/sources/vala/0.20/vala-0.20.0.tar.xz)


> **Spoilers:** 32-bit versions are known to be more stable at the time of this writing.

### Using download-mingw-rpm.py

* Launch `cmd.exe` and navigate to the folder where the script was saved.
* Make sure that Python can access `7z.exe`.
* Run the following command and wait for it to extract the packages into your current directory:
* `c:\Python33\python.exe download-mingw-rpm.py -u http://ftp.wsisiz.edu.pl/pub/linux/fedora/linux/updates/18/i386/ --deps mingw32-webkitgtk mingw32-glib-networking mingw32-gdb mingw32-gstreamer-plugins-good`

See [Fedora 18 packages](http://dl.fedoraproject.org/pub/fedora/linux/releases/18/Everything/i386/os/Packages/m/).

> **Spoilers:** Use `msys.bat` to launch a shell
