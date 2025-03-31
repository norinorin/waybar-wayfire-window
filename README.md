## waybar-wayfire-window

A waybar module that shows the current active window in a given output (similar to sway/window and hyprland/window). This uses wayfire's IPC and the foreign toplevel protocol so you should add `ipc` and `foreign-toplevel` to `core.plugins` in your wayfire ini.

## Building

To build it, run:
```sh
make
```

Once you get the binary, see the [example config](https://github.com/norinorin/waybar-wayfire-window/tree/main/example_config) to get started.