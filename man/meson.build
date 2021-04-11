xsltproc = find_program('xsltproc', required: false)
assert(xsltproc.found(), 'xsltproc is required for man pages generation')

xsltproc_cmd = [
  xsltproc,
  '--output', '@OUTPUT@',
  '--stringparam', 'man.output.quietly', '1',
  '--stringparam', 'funcsynopsis.style', 'ansi',
  '--stringparam', 'man.th.extra1.suppress', '1',
  '--stringparam', 'man.authors.section.enabled', '0',
  '--stringparam', 'man.copyright.section.enabled', '0',
  'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
  '@INPUT@',
]

mans = [
  'glade',
  'glade-previewer',
]

foreach man: mans
  output = man + '.1'

  custom_target(
    output,
    input: man + '.xml',
    output: output,
    command: xsltproc_cmd,
    install: true,
    install_dir: glade_mandir / 'man1',
  )
endforeach
