kind: import

description: |
  Import GNOME SDK

sources:
- kind: ostree
  url: gnomesdk:repo/
  gpg-key: keys/gnome-sdk.gpg
  (?):
  - arch == "x86_64":
      track: runtime/org.gnome.Sdk/x86_64/3.26
      ref: ff23c4387890886f8fe6a2a1650efaab16a17e260cc06185aa161ea59ebfecb0
  - arch == "i386":
      track: runtime/org.gnome.Sdk/i386/3.26
  - arch == "arm":
      track: runtime/org.gnome.Sdk/arm/3.26
  - arch == "aarch64":
      track: runtime/org.gnome.Sdk/aarch64/3.26

config:
  source: files
  target: usr
