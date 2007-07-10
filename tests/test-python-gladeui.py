
import pygtk
import gtk
import gladeui

class Window(gtk.Window):

    def __init__(self):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.connect("destroy", self.quit)
        self.connect("delete_event", self.quit)

	# initialise gladeui app        
        self.app = gladeui.App()

	# add palette to window        
        self.palette = self.app.get_palette()
        self.palette.set_show_selector_button (True)
 
        self.add(self.palette)
        self.palette.show()
        
        # create a new project
        self.project = gladeui.Project()
        self.app.add_project (self.project)


    def quit(self, *args):
        self.hide()
        gtk.main_quit()
        return

if __name__ == '__main__':
    window = Window()
    window.show()
    gtk.main()
    
