# hyprNStack
This plugin is a modified version of Hyprland's Master layout. 

The primary change is that it allows an arbitrary number of non-master 'stacks'. This can be changed dynamically per-workspace.

The layout is sort of a combination of XMonad's 'MultiColumns' and Hyprland's Master layout.

# Configuration
Default values are meant to produce a similar experience to the existing Master layout.
```
plugin {
  nstack {
    layout {
      orientation=left
      new_on_top=0
      new_is_master=1
      no_gaps_when_only=0
      special_scale_factor=0.8
      inherit_fullscreen=1
      stacks=2
      center_single_master=0
      mfact=0.5
      single_mfact=0.5
    }
  }
}
```

### Configuration variable differences in comparison to Master Layout
*  `stacks` The number of *total* stacks, including the master.
*  `mfact` If this is set to 0 the master is the same size as the stacks. So if there is one master and 2 stacks they are all 1/3rd of the screen width(or height). Master and 3 stacks they are all 1/4th etc.
*  `single_mfact` The size of a single centered master window, when center_single_master is set.  
* `center_single_master` When there is a single window on the screen it is centered instead of taking up the entire monitor. This replaces the existing `always_center_master` and has slightly different behavior.

### Workspace layout options
All configuration variables are also usable as workspace rule layout options. Just prefix the setting name with 'nstack-'
`workspace=2,layoutopt:nstack-stacks:2,layoutopt:nstack-single_mfact:0.85`

# Dispatchers

All dispatchers are invoked via `layoutmsg`, e.g. `layoutmsg setstackcount 3`

### Stack Management
 * `resetsplits` Reset all the window splits to default sizes.
 * `setstackcount <n|+n|-n>` Change the number of stacks for the current workspace. Windows will be re-tiled to fit the new stack count. Supports absolute values or relative adjustments (`+1`, `-1`).

### Window Movement
 * `movetostack <l|left|r|right>` Move the focused window to an adjacent slave stack. Movement wraps around (rightmost → leftmost and vice versa). Skips the master stack. No-op if the focused window is the master.
 * `swapwithmaster [master|child|auto]` Swap the focused slave window with the master window. If master is focused, swaps with the first slave.

### Stack Layout Modes
Each slave stack can independently use either vertical stacking or dwindle layout.

 * `stacklayoutmode <toggle|vertical|dwindle>` Change the layout mode for the current stack.
   - `vertical` (default): Windows are stacked linearly (vertically or horizontally based on orientation)
   - `dwindle`: Windows use recursive binary splits within the stack, similar to the dwindle layout

### Orientation
 * `orientationleft`, `orientationright`, `orientationtop`, `orientationbottom` Set master position.
 * `orientationhcenter` Master is horizontally centered with stacks to the left and right.
 * `orientationvcenter` Master is vertically centered with stacks on the top and bottom.
 * `orientationcenter` An alias for `orientationhcenter`
 * `orientationnext`, `orientationprev` Cycle through orientations.
 * `orientationcycle <orientation list>` Cycle through a specific list of orientations.

### Focus
 * `focusmaster [master|auto]` Focus the master window. If already on master, focus first slave.
 * `cyclenext`, `cycleprev` Cycle focus through windows.
 * `swapnext`, `swapprev` Swap focused window with next/previous.

# Smart Window Routing

New windows are intelligently routed based on focus:
- **Focused on a slave stack**: New window opens in the *same* stack as the focused window
- **Focused on master**: New window auto-balances across slave stacks (default behavior)

This allows you to quickly spawn multiple related windows in the same stack by keeping focus there.

# Stack Persistence

Windows remember their assigned stack across re-layouts. When you move a window to a specific stack with `movetostack`, it stays there even when:
- Other windows are added or removed
- The layout is recalculated
- Splits are reset
 

# Example Keybindings

```conf
# Stack count
bindd = SUPER CTRL, minus, Decrease stack count, layoutmsg, setstackcount -1
bindd = SUPER CTRL, equal, Increase stack count, layoutmsg, setstackcount +1
bindd = SUPER CTRL, R, Reset splits, layoutmsg, resetsplits

# Move windows between stacks ([ and ] keys)
bindd = SUPER CTRL, bracketleft, Move to left stack, layoutmsg, movetostack l
bindd = SUPER CTRL, bracketright, Move to right stack, layoutmsg, movetostack r

# Toggle stack layout mode
bindd = SUPER CTRL, Y, Toggle dwindle/vertical, layoutmsg, stacklayoutmode toggle

# Swap with master
bindd = SUPER CTRL, M, Swap with master, layoutmsg, swapwithmaster
```

# Installing

## Hyprpm, Hyprland's official plugin manager (recommended)
1. Run `hyprpm add https://github.com/zakk4223/hyprNStack` and wait for hyprpm to build the plugin.
2. Run `hyprpm enable hyprNStack`
3. Set your hyprland layout to `nstack`.

## Manual
Hyprland plugins basically have to be user-compiled and loaded. You probably need to compile and install hyprland yourself (if not using a package that exports the headers, e.g. the one on Arch's official repos).
 
If your package does not export headers, see the [this part of the hyprland wiki](https://wiki.hyprland.org/Plugins/Using-Plugins/#preparing-hyprland-sources-for-plugins)

Then:

1. Build hyprNStack
   - `make`
2. Copy the resulting nstackLayoutPlugin.so to some place
   - `cp nstackLayoutPlugin.so ~/.config/hypr/plugins`
3. Modify your hyprland.conf to load the plugin
   - `exec-once=hyprctl plugin load $HOME/.config/hypr/plugins/nstackLayoutPlugin.so`
4. Set your hyprland layout to `nstack`. 

## Plugin-Manager Hyprload
Installing via [hyprload](https://github.com/Duckonaut/hyprload) is supported.


1. Add the following to your `hyprload.toml` once `hyprload` [is running](https://github.com/Duckonaut/hyprload#installing):
``` toml
plugins = [
    "zakk4223/hyprNStack",
    { local = "https://github.com/zakk4223/hyprNStack", branch = "main", name = "hyprNStack" },
]
```
2. Reload/Update your plugins and set your hyprland layout to `nstack`.

# TODO
- [ ] Improve mouse resizing of stacks
- [X] Improve drag and drop rearranging of windows in and between stacks
- [X] Allow resizing of single master window
- [X] Move windows between stacks with `movetostack`
- [X] Smart new window routing based on focus
- [X] Per-stack layout modes (vertical/dwindle)
- [X] Stack persistence across re-layouts
