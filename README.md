[![CircleCI](https://circleci.com/gh/midori-browser/core.svg?style=svg)](https://circleci.com/gh/midori-browser/core)
[![Snap Status](https://build.snapcraft.io/badge/midori-browser/core.svg)](https://build.snapcraft.io/user/midori-browser/core)
[![FlatHub](https://img.shields.io/badge/FlatHub-gray.svg)](https://flathub.org/apps/details/org.midori_browser.Midori)
[![Telegram](https://img.shields.io/badge/Telegram-Chat-gray.svg?style=flat&logo=telegram&colorA=5583a4&logoColor=fff)](https://www.midori-browser.org/telegram)
[![Twitter](https://img.shields.io/twitter/follow/midoriweb.svg?style=social&label=Follow)](https://twitter.com/midoriweb)
[![Donate](https://img.shields.io/badge/PayPal-Donate-gray.svg?style=flat&logo=paypal&colorA=0071bb&logoColor=fff)](https://www.midori-browser.org/donate)
[![BountySource](https://img.shields.io/bountysource/team/midori/activity.svg)](https://www.bountysource.com/teams/midori)
[![Patreon](https://img.shields.io/badge/PATREON-Pledge-red.svg)](https://www.patreon.com/midoribrowser)

<p align="center">
    <img src="icons/scalable/apps/org.midori_browser.Midori.svg"/>
</p>

<p align="center">
    <b>Midori</b>
    a lightweight, fast and free web browser
</p>

![Midori Screenshot](https://www.midori-browser.org/images/screenshots/rdio.png)

Midori is a lightweight yet powerful web browser which runs just as well on little embedded computers named for delicious pastries as it does on beefy machines with a core temperature exceeding that of planet earth. And it looks good doing that, too. Oh, and of course it's free software.

**Privacy out of the box**

* Adblock filter list support
* Private browsing
* Manage cookies and scripts

**Productivity features**

* Open a 1000 tabs instantly
* Easy web apps creation
* Customizable side panels
* User scripts and styles a la Greasemonkey
* Web developer tools powered by WebKit
* Cross-browser extensions compatible with Chrome, Firefox, Opera and Vivaldi

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

You can also install Midori from [FlatHub](https://flathub.org/apps/details/org.midori_browser.Midori).

    flatpak install flathub org.midori_browser.Midori

# Installing Midori on Android

You can opt-in for the [beta release on the Play Store](https://play.google.com/apps/testing/org.midori_browser.midori).

# Building from source

**Requirements**

* [GLib](https://wiki.gnome.org/Projects/GLib) 2.46.2
* [GTK](https://www.gtk.org) 3.12
* [WebKitGTK](https://webkitgtk.org/) 2.16.6
* [libsoup](https://wiki.gnome.org/Projects/libsoup) 2.48.0
* [sqlite](https://sqlite.org) 3.6.19
* [Vala](https://wiki.gnome.org/Projects/Vala) 0.30
* GCR 2.32
* [Libpeas](https://wiki.gnome.org/Projects/Libpeas)
* [JSON-Glib](https://wiki.gnome.org/Projects/JsonGlib) 0.12

Install dependencies on Astian OS, Ubuntu, Debian or other Debian-based distros:

    sudo apt install cmake valac libwebkit2gtk-4.0-dev libgcr-3-dev libpeas-dev libsqlite3-dev libjson-glib-dev libarchive-dev intltool libxml2-utils

Install dependencies on openSUSE:

    sudo zypper in cmake vala gcc webkit2gtk3-devel libgcr-devel libpeas-devel sqlite3-devel json-glib-devel libarchive-devel fdupes gettext-tools intltool libxml2-devel

Install dependencies on Fedora:

    sudo dnf install gcc cmake intltool vala libsoup-devel sqlite-devel webkit2gtk3-devel gcr-devel json-glib-devel libpeas-devel libarchive-devel libxml2-devel

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

We're on a 8/4 cycle which means 8 weeks of features and 4 weeks of stabilization
capped at a release once every 3 months ie. at the last of the third month.

Update `CORE_VERSION` in `CMakeLists.txt` to `10.0`.
Add a section to `CHANGELOG.md`.
Add release to `data/org.midori_browser.Midori.appdata.xml.in`.

    git commit -p -v -m "Release Midori 10.0"
    git checkout -B release-10.0
    git push origin HEAD
    git archive --prefix=midori-v10.0/ -o midori-v10.0.tar.gz -9 HEAD

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

# Extensions

## Cross-browser web extensions

The following API specification is supported by Midori:

    manifest.json
      name
      version
      description
      background:
        page: *.html
        scripts:
        - *.js
      browser_action:
        default_popup: *.html
        default_icon: *.png
        default_title
      sidebar_action:
        default_panel: *.html
        default_icon: *.png
        default_title
      content_scripts:
        js:
        - *.js
        css:
        - *.css
      manifest_version: 2

    *.js
      browser (chrome)
        tabs
          create
          - url: uri
          executeScript
          - code: string
        notifications
          create
          - title: string
            message: string

# Jargon

* **freeze**: a period of bug fixes eg. 4/2 cycle means 4 weeks of features and 2 weeks of stabilization
* **PR**: pull request, a branch proposed for review, analogous to **MR** (merge request) with Bazaar
* **ninja**: an internal tab, usually empty label, used for taking screenshots
* **fortress**: user of an ancient release like 0.4.3 as found on Raspberry Pie, Debian, Ubuntu
* **katze, sokoke, tabby**: legacy API names and coincidentally cat breeds
* web extension: a cross-browser extension (plugin) - or in a webkit context, the multi-process api

# Midori for Android

The easiest way to build, develop and test Midori on Android is with [Android Studio](https://developer.android.com/studio/#downloads) ([snap](https://snapcraft.io/android-studio)).

When working with the command line, setting `JAVA_HOME` is paramount:

    export JAVA_HOME=/snap/android-studio/current/android-studio/jre/

Afterwards you can run commands like so:

    ./gradlew lint test

# Midori for Windows

Check out [midori-next](https://gitlab.com/midori-browser/midori-next).

> **Spoilers:** Upstream WebKitGTK no longer supports Windows the latest code can't be built for or run on Windows. Instead there's a separate port available.
