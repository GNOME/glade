kind: autotools

description: |
  Build Glade from upstream source repo.

depends:
- filename: gnome-sdk.bst
  type: build
- filename: gnome-platform.bst
  type: runtime
- filename: base-symlinks.bst
  type: all

sources:
- kind: git
  url: git_gnome_org:glade
  track: master

variables:
  conf-local: --disable-man-pages
