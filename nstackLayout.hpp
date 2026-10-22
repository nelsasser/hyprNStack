#pragma once

#include "globals.hpp"
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/algorithm/TiledAlgorithm.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/ConfigErrors.hpp>
#include <hyprland/src/helpers/math/Math.hpp>
#include <hyprland/src/helpers/math/Direction.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <vector>
#include <list>

enum eFullscreenMode : int8_t;

// Layout mode for individual slave stacks
enum class eStackLayoutMode : uint8_t {
    VERTICAL,  // Current behavior: windows stacked vertically/horizontally based on orientation
    DWINDLE    // Recursive splits within the stack
};

// Split direction for dwindle layout nodes
enum class eSplitDirection : uint8_t {
    SPLIT_HORIZONTAL,  // Split top/bottom
    SPLIT_VERTICAL     // Split left/right
};

//orientation determines which side of the screen the master area resides
enum eColOrientation : uint8_t {
    NSTACK_ORIENTATION_LEFT = 0,
    NSTACK_ORIENTATION_TOP,
    NSTACK_ORIENTATION_RIGHT,
    NSTACK_ORIENTATION_BOTTOM,
    NSTACK_ORIENTATION_HCENTER,
    NSTACK_ORIENTATION_VCENTER,
};

struct SNstackNodeData {
    bool              isMaster       = false;
    bool              masterAdjusted = false;
    float             percMaster     = 0.5f;
    int               stackNum       = 0;
    int               targetStack    = -1;  // -1 = auto-assign, 0+ = force this stack (0-indexed)
    WP<Layout::ITarget> dwindleParent;      // Parent target for dwindle layout (target this was split from)
    eSplitDirection   splitDirection = eSplitDirection::SPLIT_HORIZONTAL; // For dwindle: direction to split this node

    WP<Layout::ITarget> pTarget;

    Vector2D          position;
    Vector2D          size;

    float             percSize = 1.f; // size multiplier for resizing children

    bool              ignoreFullscreenChecks = false;

    bool              operator==(const SNstackNodeData& rhs) const {
        return pTarget.lock() == rhs.pTarget.lock();
    }
};

struct SNstackWorkspaceData {
    std::vector<float>               stackPercs;
    std::vector<int>                 stackNodeCount;
    std::vector<eStackLayoutMode>    stackLayoutModes;  // Per-stack layout mode
    int                              m_iStackCount        = 2;
    bool               new_on_top           = false;
    bool               new_is_master        = true;
    bool               center_single_master = false;
    bool               inherit_fullscreen   = true;
    int                no_gaps_when_only    = 0;
    float              master_factor        = 0.0f;
    float              single_master_factor = 0.5f;
    float              special_scale_factor = 0.8f;
    eColOrientation    orientation          = NSTACK_ORIENTATION_LEFT;
};

class CNstackAlgorithm : public Layout::ITiledAlgorithm {
  public:
    CNstackAlgorithm()          = default;
    virtual ~CNstackAlgorithm() = default;

    // ITiledAlgorithm interface
    virtual void                             newTarget(SP<Layout::ITarget> target) override;
    virtual void                             movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt) override;
    virtual void                             removeTarget(SP<Layout::ITarget> target) override;
    virtual void                             resizeTarget(const Vector2D& delta, SP<Layout::ITarget> target, Layout::eRectCorner corner = Layout::CORNER_NONE) override;
    virtual void                             recalculate(Layout::eRecalculateReason reason = Layout::RECALCULATE_REASON_UNKNOWN) override;
    virtual SP<Layout::ITarget>              getNextCandidate(SP<Layout::ITarget> old) override;
    virtual Config::ErrorResult              layoutMsg(const std::string_view& sv) override;
    virtual std::optional<Vector2D>          predictSizeForNewTarget() override;
    virtual void                             swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) override;
    virtual void                             moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) override;

  private:
    std::list<SNstackNodeData> m_nodes;
    SNstackWorkspaceData       m_wsData;
    bool                       m_forceWarps = false;

    void                            buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, Hyprutils::String::CVarList2& vars);
    void                            buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle);
    void                            runOrientationCycle(Hyprutils::String::CVarList2* vars, int next);
    int                             getNodesCount();
    void                            applyNodeDataToWindow(SNstackNodeData*);
    void                            resetNodeSplits();
    SNstackNodeData*                getNodeFromTarget(SP<Layout::ITarget> target);
    SNstackNodeData*                getNodeFromWindow(PHLWINDOW pWindow);
    SNstackNodeData*                getMasterNode();
    void                            transferMaster(SNstackNodeData* from, SNstackNodeData* to);
    void                            applyWorkspaceLayoutOptions();
    void                            calculateWorkspace();
    SP<Layout::ITarget>             getNextTarget(SP<Layout::ITarget> target, bool next);
    int                             getMastersCount();
    bool                            isTargetTiled(SP<Layout::ITarget> target);

    void                            moveToAdjacentStack(SP<Layout::ITarget> target, int direction);

    // Slave stack layout handlers
    void                            layoutStackVertical(std::vector<SNstackNodeData*>& windows, PHLMONITOR monitor, int orientation, Vector2D stackPos, float stackWidth, float stackNodeSizeLeft);
    void                            layoutStackDwindle(std::vector<SNstackNodeData*>& windows, PHLMONITOR monitor, int orientation, Vector2D stackPos, float stackWidth, float stackNodeSizeLeft);
    void                            recomputeSplitDirections(std::vector<SNstackNodeData*>& windows, int orientation);

    friend struct SNstackNodeData;
    friend struct SNstackWorkspaceData;
};

template <typename CharT>
struct std::formatter<SNstackNodeData*, CharT> : std::formatter<CharT> {
    template <typename FormatContext>
    auto format(const SNstackNodeData* const& node, FormatContext& ctx) const {
        auto out = ctx.out();
        if (!node)
            return std::format_to(out, "[Node nullptr]");
        std::format_to(out, "[Node {:x}: pos: {:j2}, size: {:j2}", (uintptr_t)node, node->position, node->size);
        if (node->isMaster)
            std::format_to(out, ", master");
        if (!node->pTarget.expired())
            std::format_to(out, ", target: {:x}", (uintptr_t)node->pTarget.lock().get());
        return std::format_to(out, "]");
    }
};
