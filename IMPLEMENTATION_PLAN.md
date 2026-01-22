# Plan: Fork hyprNStack to Add Move-Between-Stacks + Smart Window Routing

## Problem Summary

hyprNStack has two limitations:
1. `layoutmsg movewindow l/r` doesn't actually move windows between stacks
2. New windows auto-balance across stacks regardless of focus location

## Desired Behavior

1. **Move between stacks**: Move focused window to adjacent slave stack (skips master, wraps around)
   - `SUPER+CTRL+[` = move to next slave stack left (wraps from leftmost to rightmost)
   - `SUPER+CTRL+]` = move to next slave stack right (wraps from rightmost to leftmost)
   - Example with layout `A|M|B`: moving from B left goes directly to A (skips master M)
   - Master windows cannot be moved with this command (no-op if master focused)
2. **Smart new window routing**:
   - Focused on slave stack → new window opens in THAT stack
   - Focused on master → new window auto-balances across slave stacks
3. **Per-stack layout mode**: Toggle between vertical stacking (current) and dwindle within each slave stack
4. **Swap with master**: Keybind to swap focused slave window with master (no-op if master is focused)

## Solution: Fork and Modify hyprNStack

Add two features:
1. New dispatcher `movetostack` for explicit window movement
2. Modified `onWindowCreatedTiling()` for smart window routing

## Architecture Understanding

From analyzing the source code:

1. **`SNstackNodeData`** - Each window has a `stackNum` property (0 = master, 1+ = side stacks)
2. **`calculateWorkspace()`** - Assigns `stackNum` during layout based on window list order
3. **`moveWindowTo()`** - Uses Hyprland's directional system, not stack-aware
4. **Key insight**: To move a window to a different stack, we need to:
   - Change its `stackNum` directly
   - Trigger `recalculateMonitor()` to re-layout

## Implementation Plan

### Step 1: Fork the Repository

```bash
cd ~/code  # or preferred location
git clone https://github.com/zakk4223/hyprNStack.git hyprNStack-fork
cd hyprNStack-fork
```

### Step 2: Add New Dispatcher Handler

In `nstackLayout.cpp`, add a handler for `movetostack` in the dispatcher section:

```cpp
// In the dispatcher handling section (look for existing layoutmsg handlers)
if (command == "movetostack") {
    // Parse direction: l/left or r/right
    // In hcenter orientation with 3 stacks:
    //   Stack 0 = left
    //   Master = center
    //   Stack 1 = right

    auto* PNODE = getNodeFromWindow(PWINDOW);
    if (!PNODE || PNODE->isMaster) return;  // Can't move master

    int targetStack = (direction == "l" || direction == "left") ? 0 : 1;

    // Only move if changing stacks
    if (PNODE->stackNum != targetStack + 1) {  // +1 because stackNum 0 is master concept
        PNODE->stackNum = targetStack + 1;
        recalculateMonitor(PWINDOW->m_iMonitorID);
    }
}
```

### Step 3: Handle Stack Number Persistence

The current `calculateWorkspace()` reassigns `stackNum` during layout. We need to preserve manual assignments:

**Approach: Add `manualStack` flag to `SNstackNodeData`**

In `nstackLayout.hpp`:
```cpp
struct SNstackNodeData {
    // ... existing fields ...
    int targetStack = -1;  // -1 = auto-assign, 0+ = force this stack
};
```

In `calculateWorkspace()`, modify the stack assignment loop:
```cpp
// Instead of always using modulo assignment:
if (nd.targetStack >= 0 && nd.targetStack < numStacks) {
    nd.stackNum = nd.targetStack + 1;  // Use forced stack
} else {
    nd.stackNum = (slavesTotal - slavesLeft) % numStacks + 1;  // Auto-balance
}
```

### Step 4: Implement Smart New Window Routing

Modify `onWindowCreatedTiling()` to route new windows based on focus:

```cpp
void CHyprNstackLayout::onWindowCreatedTiling(CWindow* pWindow, ...) {
    // ... existing code to create node ...

    // Determine where to place new window
    auto* PFOCUSED = g_pCompositor->m_pLastWindow;
    if (PFOCUSED && PFOCUSED->m_iWorkspaceID == pWindow->m_iWorkspaceID) {
        auto* PFOCUSEDNODE = getNodeFromWindow(PFOCUSED);

        if (PFOCUSEDNODE && !PFOCUSEDNODE->isMaster) {
            // Focused on slave stack - route new window to same stack
            PNODE->targetStack = PFOCUSEDNODE->stackNum - 1;  // Convert to 0-indexed
        }
        // else: focused on master - leave targetStack = -1 for auto-balance
    }

    // ... rest of existing code ...
}
```

### Step 5: Implement movetostack Dispatcher (Generalized)

Support moving to adjacent stack with wraparound for arbitrary stack counts.

```cpp
void CHyprNstackLayout::moveToAdjacentStack(CWindow* pWindow, int direction) {
    // direction: -1 = left, +1 = right
    auto* PNODE = getNodeFromWindow(pWindow);
    if (!PNODE || PNODE->isMaster) return;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    if (!PWORKSPACE) return;

    auto* WSDATA = getWorkspaceData(PWORKSPACE);
    int numStacks = WSDATA->stackNodeCount.size();
    if (numStacks <= 1) return;  // Nothing to do with 1 or 0 stacks

    // Get current stack (0-indexed)
    int currentStack = PNODE->targetStack >= 0 ? PNODE->targetStack : (PNODE->stackNum - 1);

    // Calculate new stack with wraparound
    int newStack = currentStack + direction;
    if (newStack < 0) {
        newStack = numStacks - 1;  // Wrap to rightmost
    } else if (newStack >= numStacks) {
        newStack = 0;  // Wrap to leftmost
    }

    // Set the target stack
    PNODE->targetStack = newStack;

    // Trigger re-layout
    recalculateMonitor(pWindow->m_iMonitorID);
}
```

In the dispatcher handler:
```cpp
if (command == "movetostack") {
    auto* PWINDOW = g_pCompositor->m_pLastWindow;
    if (!PWINDOW) return;

    // Parse direction: l/left = -1, r/right = +1
    int direction = (args == "l" || args == "left") ? -1 : 1;
    moveToAdjacentStack(PWINDOW, direction);
}
```

### Step 6: Implement Per-Stack Layout Mode

Add support for switching between vertical stacking and dwindle layout within each slave stack.

**Add layout mode enum and per-stack tracking:**

In `nstackLayout.hpp`:
```cpp
enum class StackLayoutMode {
    VERTICAL,  // Current behavior: windows stacked vertically
    DWINDLE    // Recursive splits within the stack
};

struct SNstackWorkspaceData {
    // ... existing fields ...
    std::vector<StackLayoutMode> stackLayoutModes;  // Per-stack layout mode
};
```

**Add dispatcher to toggle mode:**

```cpp
if (command == "stacklayoutmode") {
    // Toggle layout mode for current stack
    // args: "toggle", "vertical", or "dwindle"

    auto* PWINDOW = g_pCompositor->m_pLastWindow;
    if (!PWINDOW) return;

    auto* PNODE = getNodeFromWindow(PWINDOW);
    if (!PNODE || PNODE->isMaster) return;

    int stackIndex = PNODE->stackNum - 1;
    auto* WSDATA = getWorkspaceData(PWORKSPACE);

    if (args == "toggle") {
        WSDATA->stackLayoutModes[stackIndex] =
            (WSDATA->stackLayoutModes[stackIndex] == StackLayoutMode::VERTICAL)
            ? StackLayoutMode::DWINDLE : StackLayoutMode::VERTICAL;
    } else if (args == "dwindle") {
        WSDATA->stackLayoutModes[stackIndex] = StackLayoutMode::DWINDLE;
    } else {
        WSDATA->stackLayoutModes[stackIndex] = StackLayoutMode::VERTICAL;
    }

    recalculateMonitor(PWINDOW->m_iMonitorID);
}
```

**Modify `calculateWorkspace()` to use stack layout mode:**

When laying out windows in a stack, check the mode:
```cpp
if (WSDATA->stackLayoutModes[stackIndex] == StackLayoutMode::DWINDLE) {
    // Use dwindle algorithm for this stack's windows
    layoutStackDwindle(stackWindows, stackBox);
} else {
    // Current behavior: vertical stacking
    layoutStackVertical(stackWindows, stackBox);
}
```

**Implement dwindle layout for stack:**

```cpp
void layoutStackDwindle(std::vector<SNstackNodeData*>& windows, CBox& stackBox) {
    // Recursive split: first window takes half, rest dwindle in other half
    // Alternate horizontal/vertical splits
    if (windows.empty()) return;
    if (windows.size() == 1) {
        windows[0]->position = stackBox.pos();
        windows[0]->size = stackBox.size();
        return;
    }

    // Split direction alternates (start with horizontal for vertical stack feel)
    bool horizontal = true;
    CBox remaining = stackBox;

    for (size_t i = 0; i < windows.size(); i++) {
        if (i == windows.size() - 1) {
            // Last window takes remaining space
            windows[i]->position = remaining.pos();
            windows[i]->size = remaining.size();
        } else {
            CBox thisBox = remaining;
            if (horizontal) {
                thisBox.h = remaining.h / 2;
                remaining.y += thisBox.h;
                remaining.h -= thisBox.h;
            } else {
                thisBox.w = remaining.w / 2;
                remaining.x += thisBox.w;
                remaining.w -= thisBox.w;
            }
            windows[i]->position = thisBox.pos();
            windows[i]->size = thisBox.size();
            horizontal = !horizontal;
        }
    }
}
```

### Step 7: Swap With Master (Already Exists!)

**Good news:** The `swapwithmaster` command already works with hyprNStack (inherited from base Master layout). Tested and confirmed - no custom implementation needed.

Just add a keybind:
```conf
bindd = SUPER CTRL, M, Swap with master, layoutmsg, swapwithmaster
```

**Note:** When adding our `targetStack` field, we may need to ensure that when windows swap master status, the old master inherits the slave's `targetStack` so it lands in the correct stack. This might require a small hook or modification to preserve stack assignment after swap.

### Step 8: Build and Test

```bash
# Build the plugin
make all

# Install locally (creates symlink to built .so)
hyprpm add .
hyprpm enable hyprNStack

# Or manual install
cp nstackLayoutPlugin.so ~/.local/share/hyprload/plugins/
```

### Step 9: Add Keybinds

```conf
# In bindings.conf - nstack enhancements
bindd = SUPER CTRL, bracketleft, Move window to stack on left, layoutmsg, movetostack l
bindd = SUPER CTRL, bracketright, Move window to stack on right, layoutmsg, movetostack r
bindd = SUPER CTRL, Y, Toggle stack dwindle/vertical, layoutmsg, stacklayoutmode toggle
bindd = SUPER CTRL, M, Swap with master, layoutmsg, swapwithmaster
```

Note: `bracketleft` = `[`, `bracketright` = `]`. Movement wraps around at edges.

## Files to Modify in Fork

| File | Changes |
|------|---------|
| `nstackLayout.hpp` | Add `targetStack` field to `SNstackNodeData`, add `StackLayoutMode` enum, add `stackLayoutModes` to workspace data, add `moveToStack()` method |
| `nstackLayout.cpp` | Implement: (1) `movetostack` dispatcher, (2) `stacklayoutmode` dispatcher, (3) modified stack assignment in `calculateWorkspace()`, (4) smart routing in `onWindowCreatedTiling()`, (5) dwindle layout algorithm for stacks, (6) optional hook for targetStack preservation on swapwithmaster |
| `README.md` | Document new dispatchers and behaviors |

## Key Code Sections to Study

1. **Dispatcher handling**: Search for `SLayoutMessageHeader` and existing `layoutmsg` handlers
2. **Stack assignment**: `calculateWorkspace()` function - modify to respect `targetStack`
3. **Window creation**: `onWindowCreatedTiling()` - add focus-based routing logic
4. **Node data structure**: `SNstackNodeData` - add `targetStack` field

## Complexity Assessment

- **Estimated LOC**: 150-300 lines of new/modified code
- **Features**:
  1. `movetostack` dispatcher (~40 lines)
  2. `targetStack` persistence in `calculateWorkspace()` (~20 lines)
  3. Smart routing in `onWindowCreatedTiling()` (~30 lines)
  4. `stacklayoutmode` dispatcher (~30 lines)
  5. Dwindle layout algorithm for stacks (~60 lines)
  6. `swapwithmaster` - already exists! (just add keybind, possibly small hook for targetStack)
- **Risk areas**:
  - Ensuring `targetStack` persists through re-layout cycles
  - Correctly identifying focused window's stack during new window creation
  - Dwindle algorithm matching expected behavior
  - Edge cases: empty stacks, master-only, single window, mode persistence

## Verification

### Test 1: Move Between Stacks
1. Build plugin successfully
2. Load with hyprpm
3. Open 3 windows on workspace 6 (left/center/right)
4. Focus window in left stack
5. Press `SUPER+CTRL+]` → window should move to right stack
6. Verify: left stack has 0 windows, right stack has 2 windows

### Test 1b: Movement Skips Master
1. Layout: A|M|B (window in A, master M, window in B)
2. Focus window in B
3. Press `SUPER+CTRL+[` → window moves directly to A (skips master)
4. Verify: A now has 2 windows, master unchanged

### Test 1c: Wraparound Movement
1. Have window in rightmost stack (only slave stacks, not master)
2. Press `SUPER+CTRL+]` → window should wrap to leftmost slave stack
3. Press `SUPER+CTRL+[` → window should wrap back to rightmost slave stack

### Test 1d: Master Cannot Be Moved
1. Focus the master window
2. Press `SUPER+CTRL+[` or `SUPER+CTRL+]`
3. Verify: nothing happens (no-op for master)

### Test 2: New Window Routing (Focused on Slave Stack)
1. Have 3 windows (left/center/right)
2. Focus the left stack window
3. Open a new terminal
4. Verify: new window opens in LEFT stack (now 2 windows in left)

### Test 3: New Window Routing (Focused on Master)
1. Have 3 windows (left/center/right)
2. Focus the center (master) window
3. Open a new terminal
4. Verify: new window auto-balances (goes to stack with fewer windows)

### Test 4: Persistence After Re-layout
1. Move a window to right stack with `SUPER+CTRL+L`
2. Resize the window or trigger re-layout
3. Verify: window stays in right stack (targetStack is preserved)

### Test 5: Stack Layout Mode Toggle
1. Have 3+ windows in the left stack (vertical stacking)
2. Focus a window in the left stack
3. Press `SUPER+CTRL+D` to toggle to dwindle
4. Verify: windows rearrange into dwindle pattern (recursive splits)
5. Press `SUPER+CTRL+D` again
6. Verify: windows return to vertical stacking

### Test 6: Independent Stack Modes
1. Set left stack to dwindle mode
2. Set right stack to vertical mode
3. Verify: each stack maintains its own layout mode independently

### Test 7: Swap With Master
1. Have 3 windows (left/center/right)
2. Focus the left stack window
3. Press `SUPER+CTRL+M`
4. Verify: left window is now in center (master), old master is in left stack
5. Focus the new master (center)
6. Press `SUPER+CTRL+M`
7. Verify: nothing happens (already master)

## Maintenance Considerations

- Subscribe to hyprNStack releases for upstream changes
- Hyprland plugin API changes may require updates
- Consider submitting PR upstream if feature works well
