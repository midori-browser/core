#!/bin/bash
# Copyright (C) 2015 Paweł Forysiuk <tuxator@o2.pl>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# See the file COPYING for the full license text.

BZR_PLUGINS_DIR=${HOME}/.bazaar/plugins/
if [ ! -d $BZR_PLUGINS_DIR ]; then
    mkdir -p $BZR_PLUGINS_DIR
fi
    cd $BZR_PLUGINS_DIR

    echo
    echo "Fetching pager plugin..."
    bzr branch lp:bzr-pager pager

    echo
    echo "Fetching bisect plugin..."
    bzr branch lp:bzr-bisect bisect

#    http://blog.mariadb.org/bzr-hacks-and-tricks/
    echo
    echo "Fetching diffoptions plugin..."
    mkdir diffoptions
    cat >> diffoptions/__init__.py << _EOF
    #!/usr/bin/env python

"""global bzr diff options"""

version_info = (0, 0, 1)

from bzrlib import branch
from bzrlib import diff
from bzrlib import errors

def get_differ(tree):
  try:
    root = tree.id2path(tree.get_root_id())
    br = branch.Branch.open_containing(root)[0]
    diff_options = br.get_config().get_user_option('diff_options')
    if diff_options is not None:
      opts = diff_options.split()
      def diff_file(olab, olines, nlab, nlines, to_file, path_encoding=None):
          diff.external_diff(olab, olines, nlab, nlines, to_file, opts)
      return diff_file
  except (errors.NoSuchId, NotImplementedError):
    pass
  return None

old_init = diff.DiffText.__init__

def new_init(self, old_tree, new_tree, to_file, path_encoding='utf-8',
    old_label='', new_label='', differ=diff.internal_diff):

  if differ == diff.internal_diff:
    differ = get_differ(old_tree) or get_differ(new_tree) or diff.internal_diff

  old_init(self, old_tree, new_tree, to_file, path_encoding,
      old_label, new_label, differ)

diff.DiffText.__init__ = new_init
_EOF

cat <<_EOF

Add this to your ~/.bazaar/bazaar.conf

[DEFAULT]
diff_options='-u -F ^[[:alpha:]$_].*[^:]$'

_EOF
