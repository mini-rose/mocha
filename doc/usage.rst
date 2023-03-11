mocha
=====

Package manager is part of mocha-lang project.

It contains basic operations for creating mocha packages.


legend
------
    `root` - root directory (package's home dir)


package
-------

Creating new package:
    `$ mocha new <root>`
    - mocha will create package template in root

Removing cache and binaries:
    `$ mocha clean`

Testing our package:
    `$ mocha run`
    - it will compile all files in src directory with all subdirs
