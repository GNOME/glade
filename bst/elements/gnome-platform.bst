kind: import

description: |
  Import GNOME runtime

sources:
- kind: ostree
  url: gnomesdk:repo/
  gpg-key: keys/gnome-sdk.gpg
  (?):
  - arch == "x86_64":
      track: runtime/org.gnome.Platform/x86_64/3.26
      ref: 05052b2520aef1273b724c8f776ee1042975209d5a2e3fc4aa02cb1fad95a856
  - arch == "i386":
      track: runtime/org.gnome.Platform/i386/3.26
  - arch == "arm":
      track: runtime/org.gnome.Platform/arm/3.26
  - arch == "aarch64":
      track: runtime/org.gnome.Platform/aarch64/3.26

config:
  source: files
  target: usr
