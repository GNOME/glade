#!/usr/bin/gjs

const GObject = imports.gi.GObject;
const Gtk     = imports.gi.Gtk;

var MyJSGrid = GObject.registerClass({
    GTypeName: 'MyJSGrid',
}, class MyJSGrid extends Gtk.Grid {
    _init() {
        super._init();
    }
});

