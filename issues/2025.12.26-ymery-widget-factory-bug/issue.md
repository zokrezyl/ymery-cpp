Bug in ymery-cpp: WidgetFactory destructor causes segfault on exit

When yetty exits after using the ymery plugin, there's a segfault in ymery's WidgetFactory destructor.

## Backtrace

```
#0  std::_Sp_counted_ptr<ymery::WidgetFactory*>::_M_dispose() from libymery.so
#1  std::_Sp_counted_base::_M_release_last_use_cold()
#2  std::_Sp_counted_ptr<ymery::EmbeddedApp*>::_M_dispose() from libymery.so
#3  yetty::YmeryPlugin::dispose()
#4  yetty::PluginManager::~PluginManager()
#5  main()
```

## Analysis

The crash happens when EmbeddedApp is destroyed, which triggers WidgetFactory destructor.

Likely causes:
1. WidgetFactory holds references to components that are destroyed in wrong order
2. Circular shared_ptr references between WidgetFactory, widgets, and other components
3. WidgetFactory destructor accesses freed memory (use-after-free)

## EmbeddedApp member destruction order

```cpp
// Destroyed in reverse declaration order:
std::shared_ptr<Lang> _lang;              // last
std::shared_ptr<Dispatcher> _dispatcher;
std::shared_ptr<WidgetFactory> _widget_factory;  // CRASH HERE
std::shared_ptr<PluginManager> _plugin_manager;
std::shared_ptr<TreeLike> _data_tree;
WidgetPtr _root_widget;                   // first
```

When `_widget_factory` is destroyed, `_plugin_manager` is already destroyed.
WidgetFactory holds `_plugin_manager` shared_ptr, but the widgets it created may have raw pointers or weak refs to things that are gone.

## Workaround

Currently leaking the EmbeddedApp to avoid crash:
```cpp
// FIXME: ymery-cpp has a bug in WidgetFactory destructor
// Don't reset app - let it leak for now until ymery-cpp is fixed
// _app.reset();
```

## Fix needed in ymery-cpp

1. Check WidgetFactory destructor for dangling references
2. Consider explicit dispose() that clears internal state before destruction
3. Review destruction order of EmbeddedApp members
