import gobject
import gtk
import gladeui

class MyBoxAdaptor(gladeui.get_adaptor_for_type('GtkHBox')):
        __gtype_name__ = 'MyBoxAdaptor'
        
        def __init__(self):
                gladeui.GladeGtkHBoxAdaptor.__init__(self)
                print "__init__\n"
                
        def do_post_create(self, obj, reason):
                print "do_post_create\n"
                
        def do_child_set_property(self, container, child, property_name, value):
                gladeui.GladeGtkHBoxAdaptor.do_child_set_property(self, container, child, property_name, value)
                print "do_child_set_property\n"
                
        def do_child_get_property(self, container, child, property_name):
                a = gladeui.GladeGtkHBoxAdaptor.do_child_get_property(self, container, child, property_name)
                print "do_child_get_property\n"
                return a
                
        def do_get_children(self, container):
                a = gladeui.GladeGtkHBoxAdaptor.do_get_children(self, container)
                print "do_get_children\n"
                return a

class MyBox(gtk.HBox):
        __gtype_name__ = 'MyBox'
        
        def __init__(self):
                gtk.HBox.__init__(self)
