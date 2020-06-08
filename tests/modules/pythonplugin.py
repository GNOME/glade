from gi.repository import Gtk

class MyPythonBox(Gtk.Box):
  __gtype_name__ = 'MyPythonBox'

  def __init__ (self):
    Gtk.Box.__init__ (self)
