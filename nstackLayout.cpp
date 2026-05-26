#include "nstackLayout.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/debug/log/Logger.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/helpers/MiscFunctions.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/layout/target/WindowTarget.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/config/shared/parserUtils/ParserUtils.hpp>
#include <hyprland/src/config/shared/workspace/WorkspaceRuleManager.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <format>
#include <functional>
#include <map>
#include <hyprland/src/render/decorations/IHyprWindowDecoration.hpp>

#include <hyprutils/string/VarList2.hpp>

SNstackNodeData* CNstackAlgorithm::getNodeFromTarget(SP<Layout::ITarget> target) {
    for (auto& nd : m_nodes) {
        if (nd.pTarget.lock() == target)
            return &nd;
    }
    return nullptr;
}

SNstackNodeData* CNstackAlgorithm::getNodeFromWindow(PHLWINDOW pWindow) {
    for (auto& nd : m_nodes) {
        auto target = nd.pTarget.lock();
        if (target && target->window() == pWindow)
            return &nd;
    }
    return nullptr;
}

int CNstackAlgorithm::getNodesCount() {
    return m_nodes.size();
}

int CNstackAlgorithm::getMastersCount() {
    int no = 0;
    for (auto& n : m_nodes) {
        if (n.isMaster)
            no++;
    }
    return no;
}

bool CNstackAlgorithm::isTargetTiled(SP<Layout::ITarget> target) {
    return getNodeFromTarget(target) != nullptr;
}

void CNstackAlgorithm::applyWorkspaceLayoutOptions() {
    auto space = m_parent.lock()->space();
    auto workspace = space->workspace();

    const auto wsrule       = Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace).value_or(Config::CWorkspaceRule{});
    const auto wslayoutopts = wsrule.m_layoutopts;

    static auto* const orientation   = (Hyprlang::STRING const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:orientation")->getDataStaticPtr();
    std::string        wsorientation = *orientation;

    if (wslayoutopts.contains("nstack-orientation"))
        wsorientation = wslayoutopts.at("nstack-orientation");

    std::string cpporientation = wsorientation;
    if (cpporientation == "top") {
        m_wsData.orientation = NSTACK_ORIENTATION_TOP;
    } else if (cpporientation == "right") {
        m_wsData.orientation = NSTACK_ORIENTATION_RIGHT;
    } else if (cpporientation == "bottom") {
        m_wsData.orientation = NSTACK_ORIENTATION_BOTTOM;
    } else if (cpporientation == "left") {
        m_wsData.orientation = NSTACK_ORIENTATION_LEFT;
    } else if (cpporientation == "vcenter") {
        m_wsData.orientation = NSTACK_ORIENTATION_VCENTER;
    } else {
        m_wsData.orientation = NSTACK_ORIENTATION_HCENTER;
    }

    static auto* const NUMSTACKS = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:stacks")->getDataStaticPtr();
    auto               wsstacks  = **NUMSTACKS;

    if (wslayoutopts.contains("nstack-stacks")) {
        try {
            std::string stackstr = wslayoutopts.at("nstack-stacks");
            wsstacks             = std::stol(stackstr);
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt invalid rule value for nstack-stacks: {}", e.what()); }
    }
    if (wsstacks)
        m_wsData.m_iStackCount = wsstacks;

    static auto* const MFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:mfact")->getDataStaticPtr();
    auto               wsmfact = **MFACT;
    if (wslayoutopts.contains("nstack-mfact")) {
        try {
            wsmfact = std::stof(wslayoutopts.at("nstack-mfact"));
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-mfact format error: {}", e.what()); }
    }
    m_wsData.master_factor = wsmfact;

    static auto* const SMFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:single_mfact")->getDataStaticPtr();
    auto               wssmfact = **SMFACT;
    if (wslayoutopts.contains("nstack-single_mfact")) {
        try {
            wssmfact = std::stof(wslayoutopts.at("nstack-single_mfact"));
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-single_mfact format error: {}", e.what()); }
    }
    m_wsData.single_master_factor = wssmfact;

    static auto* const SSFACT   = (Hyprlang::FLOAT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:special_scale_factor")->getDataStaticPtr();
    auto               wsssfact = **SSFACT;
    if (wslayoutopts.contains("nstack-special_scale_factor")) {
        try {
            wsssfact = std::stof(wslayoutopts.at("nstack-special_scale_factor"));
        } catch (std::exception& e) { Log::logger->log(Log::ERR, "Nstack layoutopt nstack-special_scale_factor format error: {}", e.what()); }
    }
    m_wsData.special_scale_factor = wsssfact;

    static auto* const NEWTOP   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_on_top")->getDataStaticPtr();
    auto               wsnewtop = **NEWTOP;
    if (wslayoutopts.contains("nstack-new_on_top"))
        wsnewtop = Config::ParserUtils::parseInt(wslayoutopts.at("nstack-new_on_top")).value_or(0);
    m_wsData.new_on_top = wsnewtop;

    static auto* const NEWMASTER   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:new_is_master")->getDataStaticPtr();
    auto               wsnewmaster = **NEWMASTER;
    if (wslayoutopts.contains("nstack-new_is_master"))
        wsnewmaster = Config::ParserUtils::parseInt(wslayoutopts.at("nstack-new_is_master")).value_or(0);
    m_wsData.new_is_master = wsnewmaster;

    static auto* const NGWO   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:no_gaps_when_only")->getDataStaticPtr();
    auto               wsngwo = **NGWO;
    if (wslayoutopts.contains("nstack-no_gaps_when_only"))
        wsngwo = Config::ParserUtils::parseInt(wslayoutopts.at("nstack-no_gaps_when_only")).value_or(0);
    m_wsData.no_gaps_when_only = wsngwo;

    static auto* const INHERITFS   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:inherit_fullscreen")->getDataStaticPtr();
    auto               wsinheritfs = **INHERITFS;
    if (wslayoutopts.contains("nstack-inherit_fullscreen"))
        wsinheritfs = Config::ParserUtils::parseInt(wslayoutopts.at("nstack-inherit_fullscreen")).value_or(0);
    m_wsData.inherit_fullscreen = wsinheritfs;

    static auto* const CENTERSM   = (Hyprlang::INT* const*)HyprlandAPI::getConfigValue(PHANDLE, "plugin:nstack:layout:center_single_master")->getDataStaticPtr();
    auto               wscentersm = **CENTERSM;
    if (wslayoutopts.contains("nstack-center_single_master"))
        wscentersm = Config::ParserUtils::parseInt(wslayoutopts.at("nstack-center_single_master")).value_or(0);
    m_wsData.center_single_master = wscentersm;
}

SNstackNodeData* CNstackAlgorithm::getMasterNode() {
    for (auto& n : m_nodes) {
        if (n.isMaster)
            return &n;
    }
    return nullptr;
}

void CNstackAlgorithm::resetNodeSplits() {
    // Clear workspace data and recalculate
    m_wsData = SNstackWorkspaceData{};
    recalculate();
}

void CNstackAlgorithm::newTarget(SP<Layout::ITarget> target) {
    applyWorkspaceLayoutOptions();

    const auto PWINDOW = target->window();
    const auto PMONITOR = m_parent.lock()->space()->workspace()->m_monitor.lock();

    const auto PNODE = m_wsData.new_on_top ? &m_nodes.emplace_front() : &m_nodes.emplace_back();

    PNODE->pTarget = target;

    auto OPENINGON = isTargetTiled(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr) ?
            getNodeFromTarget(Desktop::focusState()->window()->layoutTarget()) :
            getMasterNode();

    const auto MOUSECOORDS = g_pInputManager->getMouseCoordsInternal();

    const auto WINDOWSONWORKSPACE = getNodesCount();
    float      lastSplitPercent   = 0.5f;
    bool       lastMasterAdjusted = false;

    // Drag-end deco check removed — handled by CSpace in 0.54.0

    bool newWindowIsMaster = false;
    if (m_wsData.new_is_master || WINDOWSONWORKSPACE == 1 || (!PWINDOW->m_firstMap && OPENINGON && OPENINGON->isMaster))
        newWindowIsMaster = true;

    if (newWindowIsMaster) {
        for (auto& nd : m_nodes) {
            if (nd.isMaster) {
                nd.isMaster        = false;
                lastSplitPercent   = nd.percMaster;
                lastMasterAdjusted = nd.masterAdjusted;
                break;
            }
        }

        PNODE->isMaster       = true;
        PNODE->percMaster     = lastSplitPercent;
        PNODE->masterAdjusted = lastMasterAdjusted;

        if (const auto MAXSIZE = PWINDOW->maxSize().value_or(Math::VECTOR2D_MAX); MAXSIZE.x < PMONITOR->m_size.x * lastSplitPercent || MAXSIZE.y < PMONITOR->m_size.y) {
            // Too big to tile — let the framework handle it as floating
            m_nodes.remove(*PNODE);
            return;
        }
    } else {
        PNODE->isMaster = false;

        // Smart new window routing: route to focused stack if focused on slave
        if (OPENINGON && !OPENINGON->isMaster) {
            PNODE->targetStack = OPENINGON->stackNum - 1;
            PNODE->dwindleParent = OPENINGON->pTarget;
        }

        if (const auto MAXSIZE = PWINDOW->maxSize().value_or(Math::VECTOR2D_MAX);
            MAXSIZE.x < PMONITOR->m_size.x * (1 - lastSplitPercent) || MAXSIZE.y < PMONITOR->m_size.y * (1.f / (WINDOWSONWORKSPACE - 1))) {
            m_nodes.remove(*PNODE);
            return;
        }
    }

    recalculate();
}

void CNstackAlgorithm::movedTarget(SP<Layout::ITarget> target, std::optional<Vector2D> focalPoint) {
    // A target moved into this workspace from another — treat like a new window
    newTarget(target);
}

void CNstackAlgorithm::removeTarget(SP<Layout::ITarget> target) {
    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE)
        return;

    auto PWINDOW = target->window();
    if (PWINDOW) {
        PWINDOW->m_ruleApplicator->resetProps(Desktop::Rule::RULE_PROP_ALL, Desktop::Types::PRIORITY_LAYOUT);
        PWINDOW->updateWindowData();
    }

    const auto MASTERSLEFT = getMastersCount();

    if (PNODE->isMaster && MASTERSLEFT < 2) {
        // Find a new master
        for (auto& nd : m_nodes) {
            if (!nd.isMaster && nd.pTarget.lock() != target) {
                nd.isMaster       = true;
                nd.percMaster     = PNODE->percMaster;
                nd.masterAdjusted = PNODE->masterAdjusted;
                break;
            }
        }
    }

    m_nodes.remove(*PNODE);

    if (getMastersCount() == getNodesCount() && MASTERSLEFT > 1) {
        for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); it++) {
            it->isMaster = false;
            break;
        }
    }

    recalculate();
}

void CNstackAlgorithm::recalculate(Layout::eRecalculateReason reason) {
    applyWorkspaceLayoutOptions();
    calculateWorkspace();
}

void CNstackAlgorithm::calculateWorkspace() {
    auto space = m_parent.lock()->space();
    auto workspace = space->workspace();
    const auto PMONITOR = workspace->m_monitor.lock();

    if (!PMONITOR)
        return;

    const auto WORKAREA = space->workArea();

    auto       NUMSTACKS  = m_wsData.m_iStackCount;
    const auto PMASTERNODE = getMasterNode();
    const auto NODECOUNT   = getNodesCount();

    eColOrientation orientation = m_wsData.orientation;

    if (!PMASTERNODE)
        return;

    const auto MASTERS     = getMastersCount();
    const auto ONLYMASTERS = !(NODECOUNT - MASTERS);

    if (NUMSTACKS < 3 && orientation > NSTACK_ORIENTATION_BOTTOM)
        NUMSTACKS = 3;

    if (!PMASTERNODE->masterAdjusted) {
        if (getNodesCount() < NUMSTACKS) {
            PMASTERNODE->percMaster = m_wsData.master_factor ? m_wsData.master_factor : 1.0f / getNodesCount();
        } else {
            PMASTERNODE->percMaster = m_wsData.master_factor ? m_wsData.master_factor : 1.0f / (NUMSTACKS);
        }
    }

    bool centerMasterWindow = m_wsData.center_single_master;
    if (!ONLYMASTERS && NODECOUNT - MASTERS < 2) {
        if (orientation == NSTACK_ORIENTATION_HCENTER)
            orientation = NSTACK_ORIENTATION_LEFT;
        else if (orientation == NSTACK_ORIENTATION_VCENTER)
            orientation = NSTACK_ORIENTATION_TOP;
    }

    auto MCONTAINERPOS  = Vector2D(0.0f, 0.0f);
    auto MCONTAINERSIZE = Vector2D(0.0f, 0.0f);

    if (ONLYMASTERS) {
        if (centerMasterWindow) {
            if (!PMASTERNODE->masterAdjusted)
                PMASTERNODE->percMaster = m_wsData.single_master_factor ? m_wsData.single_master_factor : 0.5f;

            if (orientation == NSTACK_ORIENTATION_TOP || orientation == NSTACK_ORIENTATION_BOTTOM) {
                const float HEIGHT        = WORKAREA.h * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (WORKAREA.h - HEIGHT) / 2;
                MCONTAINERSIZE            = Vector2D(WORKAREA.w, HEIGHT);
                MCONTAINERPOS             = WORKAREA.pos() + Vector2D(0.0, CENTER_OFFSET);
            } else {
                const float WIDTH         = WORKAREA.w * PMASTERNODE->percMaster;
                float       CENTER_OFFSET = (WORKAREA.w - WIDTH) / 2;
                MCONTAINERSIZE            = Vector2D(WIDTH, WORKAREA.h);
                MCONTAINERPOS             = WORKAREA.pos() + Vector2D(CENTER_OFFSET, 0.0);
            }
        } else {
            MCONTAINERPOS  = WORKAREA.pos();
            MCONTAINERSIZE = Vector2D(WORKAREA.w, WORKAREA.h);
        }
    } else {
        const float MASTERSIZE = orientation % 2 == 0 ? WORKAREA.w * PMASTERNODE->percMaster :
                                                         WORKAREA.h * PMASTERNODE->percMaster;

        if (orientation == NSTACK_ORIENTATION_RIGHT) {
            MCONTAINERPOS = WORKAREA.pos() + Vector2D(WORKAREA.w - MASTERSIZE, 0.0f);
            MCONTAINERSIZE = Vector2D(MASTERSIZE, WORKAREA.h);
        } else if (orientation == NSTACK_ORIENTATION_LEFT) {
            MCONTAINERPOS  = WORKAREA.pos();
            MCONTAINERSIZE = Vector2D(MASTERSIZE, WORKAREA.h);
        } else if (orientation == NSTACK_ORIENTATION_TOP) {
            MCONTAINERPOS  = WORKAREA.pos();
            MCONTAINERSIZE = Vector2D(WORKAREA.w, MASTERSIZE);
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
            MCONTAINERPOS = WORKAREA.pos() + Vector2D(0.0f, WORKAREA.h - MASTERSIZE);
            MCONTAINERSIZE = Vector2D(WORKAREA.w, MASTERSIZE);
        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            float CENTER_OFFSET = (WORKAREA.w - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(MASTERSIZE, WORKAREA.h);
            MCONTAINERPOS       = WORKAREA.pos() + Vector2D(CENTER_OFFSET, 0.0);
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            float CENTER_OFFSET = (WORKAREA.h - MASTERSIZE) / 2;
            MCONTAINERSIZE      = Vector2D(WORKAREA.w, MASTERSIZE);
            MCONTAINERPOS       = WORKAREA.pos() + Vector2D(0.0, CENTER_OFFSET);
        }
    }

    if (MCONTAINERSIZE != Vector2D(0, 0)) {
        float       nodeSpaceLeft = orientation % 2 == 0 ? MCONTAINERSIZE.y : MCONTAINERSIZE.x;
        int         nodesLeft     = MASTERS;
        float       nextNodeCoord = 0;
        const float MASTERSIZE    = orientation % 2 == 0 ? MCONTAINERSIZE.x : MCONTAINERSIZE.y;
        for (auto& n : m_nodes) {
            if (n.isMaster) {
                if (orientation == NSTACK_ORIENTATION_RIGHT) {
                    n.position = MCONTAINERPOS + Vector2D(0.0f, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_LEFT) {
                    n.position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else if (orientation == NSTACK_ORIENTATION_TOP) {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_BOTTOM) {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
                    n.position = MCONTAINERPOS + Vector2D(0.0, nextNodeCoord);
                } else {
                    n.position = MCONTAINERPOS + Vector2D(nextNodeCoord, 0.0);
                }

                float NODESIZE = nodesLeft > 1 ? nodeSpaceLeft / nodesLeft * n.percSize : nodeSpaceLeft;
                if (NODESIZE > nodeSpaceLeft * 0.9f && nodesLeft > 1)
                    NODESIZE = nodeSpaceLeft * 0.9f;

                n.size = orientation % 2 == 0 ? Vector2D(MASTERSIZE, NODESIZE) : Vector2D(NODESIZE, MASTERSIZE);
                nodesLeft--;
                nodeSpaceLeft -= NODESIZE;
                nextNodeCoord += NODESIZE;
                applyNodeDataToWindow(&n);
            }
        }
    }

    // Compute placement of slave windows
    int slavesLeft  = getNodesCount() - MASTERS;
    int slavesTotal = slavesLeft;
    if (slavesTotal < 1)
        return;

    int numStacks      = slavesTotal > NUMSTACKS - 1 ? NUMSTACKS - 1 : slavesTotal;
    int numStackBefore = numStacks / 2 + numStacks % 2;
    int numStackAfter  = numStacks / 2;

    m_wsData.stackNodeCount.assign(numStacks + 1, 0);
    m_wsData.stackPercs.resize(numStacks + 1, 1.0f);
    m_wsData.stackLayoutModes.resize(numStacks, eStackLayoutMode::VERTICAL);

    // First pass: assign stacks
    int autoAssignCounter = 0;
    for (auto& nd : m_nodes) {
        if (nd.isMaster)
            continue;

        int assignedStack;
        if (nd.targetStack >= 0 && nd.targetStack < numStacks) {
            assignedStack = nd.targetStack;
        } else {
            assignedStack = autoAssignCounter % numStacks;
            autoAssignCounter++;
        }
        nd.stackNum = assignedStack + 1;
        m_wsData.stackNodeCount[nd.stackNum]++;
    }

    float                 stackNodeSizeLeft = orientation % 2 == 1 ? WORKAREA.w : WORKAREA.h;
    std::vector<float>    nodeSpaceLeft(numStacks, stackNodeSizeLeft);
    std::vector<float>    nodeNextCoord(numStacks, 0);
    std::vector<Vector2D> stackCoords(numStacks, Vector2D(0, 0));

    const float STACKSIZE = orientation % 2 == 1 ? (WORKAREA.h - PMASTERNODE->size.y) / numStacks :
                                                    (WORKAREA.w - PMASTERNODE->size.x) / numStacks;

    const float STACKSIZEBEFORE = numStackBefore ? ((STACKSIZE * numStacks) / 2) / numStackBefore : 0.0f;
    const float STACKSIZEAFTER  = numStackAfter ? ((STACKSIZE * numStacks) / 2) / numStackAfter : 0.0f;

    if (orientation == NSTACK_ORIENTATION_LEFT || orientation == NSTACK_ORIENTATION_TOP) {
        numStackBefore = 0;
        numStackAfter  = numStacks;
    } else if (orientation == NSTACK_ORIENTATION_RIGHT || orientation == NSTACK_ORIENTATION_BOTTOM) {
        numStackAfter  = 0;
        numStackBefore = numStacks;
    }

    for (int i = 0; i < numStacks; i++) {
        float useSize = STACKSIZE;
        if (orientation > NSTACK_ORIENTATION_BOTTOM) {
            if (i < numStackBefore)
                useSize = STACKSIZEBEFORE;
            else
                useSize = STACKSIZEAFTER;
        }

        double coordAdjust = 0;
        if (i == numStackBefore && numStackAfter) {
            coordAdjust = orientation % 2 == 1 ? PMASTERNODE->position.y + PMASTERNODE->size.y - WORKAREA.y :
                                                  PMASTERNODE->position.x + PMASTERNODE->size.x - WORKAREA.x;
        }
        float monMax     = orientation % 2 == 1 ? WORKAREA.h : WORKAREA.w;
        float stackStart = 0.0f;
        if (i == numStackBefore && numStackAfter) {
            stackStart = coordAdjust;
        } else if (i) {
            stackStart = stackCoords[i - 1].y;
        }
        float scaledSize = useSize * m_wsData.stackPercs[i + 1];

        if (orientation == NSTACK_ORIENTATION_LEFT && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_RIGHT && i >= numStacks - 1) {
            scaledSize = (PMASTERNODE->position.x - WORKAREA.x) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_TOP && i >= numStacks - 1) {
            scaledSize = monMax - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_BOTTOM && i >= numStacks - 1) {
            scaledSize = (PMASTERNODE->position.y - WORKAREA.y) - stackStart;
        } else if (orientation == NSTACK_ORIENTATION_HCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.x - WORKAREA.x) - stackStart;
            }
        } else if (orientation == NSTACK_ORIENTATION_VCENTER) {
            if (i >= numStacks - 1) {
                scaledSize = monMax - stackStart;
            } else if (i == numStacks - 2) {
                scaledSize = (PMASTERNODE->position.y - WORKAREA.y) - stackStart;
            }
        }
        stackCoords[i] = Vector2D(stackStart, stackStart + scaledSize);
    }

    // Collect windows per stack for layout calculation
    std::vector<std::vector<SNstackNodeData*>> stackWindows(numStacks);
    for (auto& nd : m_nodes) {
        if (nd.isMaster)
            continue;
        int stackIdx = nd.stackNum - 1;
        if (stackIdx >= 0 && stackIdx < numStacks)
            stackWindows[stackIdx].push_back(&nd);
    }

    // Second pass: position windows
    for (int stackIdx = 0; stackIdx < numStacks; stackIdx++) {
        auto& windows = stackWindows[stackIdx];
        if (windows.empty())
            continue;

        Vector2D stackPos = stackCoords[stackIdx];
        float stackWidth = stackPos.y - stackPos.x;

        if (m_wsData.stackLayoutModes[stackIdx] == eStackLayoutMode::DWINDLE) {
            layoutStackDwindle(windows, PMONITOR, orientation, stackPos, stackWidth, stackNodeSizeLeft);
        } else {
            layoutStackVertical(windows, PMONITOR, orientation, stackPos, stackWidth, stackNodeSizeLeft);
        }
    }
}

void CNstackAlgorithm::applyNodeDataToWindow(SNstackNodeData* pNode) {
    auto target = pNode->pTarget.lock();
    if (!target)
        return;

    auto space = m_parent.lock()->space();
    auto workspace = space->workspace();
    const auto PMONITOR = workspace->m_monitor.lock();
    const auto WORKAREA = space->workArea();
    const auto PWINDOW  = target->window();

    if (!PMONITOR || !PWINDOW)
        return;

    // for gaps outer
    const bool DISPLAYLEFT   = STICKS(pNode->position.x, WORKAREA.x);
    const bool DISPLAYRIGHT  = STICKS(pNode->position.x + pNode->size.x, WORKAREA.x + WORKAREA.w);
    const bool DISPLAYTOP    = STICKS(pNode->position.y, WORKAREA.y);
    const bool DISPLAYBOTTOM = STICKS(pNode->position.y + pNode->size.y, WORKAREA.y + WORKAREA.h);

    const auto WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(workspace).value_or(Config::CWorkspaceRule{});

    if (PWINDOW->isFullscreen() && !pNode->ignoreFullscreenChecks)
        return;

    PWINDOW->m_ruleApplicator->resetProps(Desktop::Rule::RULE_PROP_ALL, Desktop::Types::PRIORITY_LAYOUT);
    PWINDOW->updateWindowData();

    static auto* const PANIMATE = (Hyprlang::INT* const*)Config::mgr()->getConfigValue("misc:animate_manual_resizes").dataptr;

    static auto* const PGAPSINDATA  = (Hyprlang::CUSTOMTYPE* const*)Config::mgr()->getConfigValue("general:gaps_in").dataptr;
    static auto* const PGAPSOUTDATA = (Hyprlang::CUSTOMTYPE* const*)Config::mgr()->getConfigValue("general:gaps_out").dataptr;
    auto* const        PGAPSIN      = (Config::CCssGapData*)(*PGAPSINDATA)->getData();
    auto* const        PGAPSOUT     = (Config::CCssGapData*)(*PGAPSOUTDATA)->getData();

    auto               gapsIn  = WORKSPACERULE.m_gapsIn.value_or(*PGAPSIN);
    auto               gapsOut = WORKSPACERULE.m_gapsOut.value_or(*PGAPSOUT);

    if (!validMapped(PWINDOW)) {
        Log::logger->log(Log::ERR, "Node {} holding invalid window {}!!", pNode, PWINDOW);
        return;
    }

    PWINDOW->m_size     = pNode->size;
    PWINDOW->m_position = pNode->position;

    if (m_wsData.no_gaps_when_only && !g_pCompositor->isWorkspaceSpecial(workspace->m_id) &&
        (getNodesCount() == 1 || PWINDOW->isEffectiveInternalFSMode(FSMODE_MAXIMIZED))) {
        PWINDOW->updateWindowDecos();
        const auto RESERVED = PWINDOW->getFullWindowReservedArea();

        target->setPositionGlobal(CBox{PWINDOW->m_position + RESERVED.topLeft, PWINDOW->m_size - (RESERVED.topLeft + RESERVED.bottomRight)});

        if (m_forceWarps && !**PANIMATE)
            target->warpPositionSize();

        return;
    }

    auto       calcPos  = PWINDOW->m_position;
    auto       calcSize = PWINDOW->m_size;

    const auto OFFSETTOPLEFT = Vector2D((double)(DISPLAYLEFT ? gapsOut.m_left : gapsIn.m_left), (double)(DISPLAYTOP ? gapsOut.m_top : gapsIn.m_top));

    const auto OFFSETBOTTOMRIGHT = Vector2D((double)(DISPLAYRIGHT ? gapsOut.m_right : gapsIn.m_right), (double)(DISPLAYBOTTOM ? gapsOut.m_bottom : gapsIn.m_bottom));

    calcPos  = calcPos + OFFSETTOPLEFT;
    calcSize = calcSize - OFFSETTOPLEFT - OFFSETBOTTOMRIGHT;

    const auto RESERVED = PWINDOW->getFullWindowReservedArea();
    calcPos             = calcPos + RESERVED.topLeft;
    calcSize            = calcSize - (RESERVED.topLeft + RESERVED.bottomRight);

    if (g_pCompositor->isWorkspaceSpecial(workspace->m_id)) {
        CBox wb = {calcPos + (calcSize - calcSize * m_wsData.special_scale_factor) / 2.f, calcSize * m_wsData.special_scale_factor};
        wb.round();
        target->setPositionGlobal(wb);
    } else {
        CBox wb = {calcPos, calcSize};
        wb.round();
        target->setPositionGlobal(wb);
    }

    if (m_forceWarps && !**PANIMATE) {
        g_pHyprRenderer->damageWindow(PWINDOW);
        target->warpPositionSize();
        g_pHyprRenderer->damageWindow(PWINDOW);
    }

    PWINDOW->updateWindowDecos();
}

void CNstackAlgorithm::layoutStackDwindle(std::vector<SNstackNodeData*>& windows, PHLMONITOR monitor, int orientation, Vector2D stackPos, float stackWidth, float stackNodeSizeLeft) {
    const auto WORKAREA = m_parent.lock()->space()->workArea();

    CBox stackBox;
    if (orientation % 2 == 0) {
        stackBox = CBox(
            WORKAREA.x + stackPos.x,
            WORKAREA.y,
            stackWidth,
            stackNodeSizeLeft
        );
    } else {
        stackBox = CBox(
            WORKAREA.x,
            WORKAREA.y + stackPos.x,
            stackNodeSizeLeft,
            stackWidth
        );
    }

    // Build parent->children map
    std::map<SP<Layout::ITarget>, std::vector<SNstackNodeData*>> childrenMap;
    std::vector<SNstackNodeData*> roots;

    for (auto* nd : windows) {
        auto parent = nd->dwindleParent.lock();
        bool parentInStack = false;
        if (parent) {
            for (auto* other : windows) {
                if (other->pTarget.lock() == parent) {
                    parentInStack = true;
                    break;
                }
            }
        }

        if (parentInStack) {
            childrenMap[parent].push_back(nd);
        } else {
            roots.push_back(nd);
        }
    }

    std::function<void(SNstackNodeData*, std::vector<SNstackNodeData*>, CBox)> layoutDwindleRecursive =
        [&](SNstackNodeData* node, std::vector<SNstackNodeData*> nodeChildren, CBox box) {
            if (nodeChildren.empty()) {
                node->position = Vector2D(box.x, box.y);
                node->size = Vector2D(box.w, box.h);
                applyNodeDataToWindow(node);
            } else {
                CBox parentBox = box;
                CBox firstChildBox = box;
                bool splitHorizontal = (node->splitDirection == eSplitDirection::SPLIT_HORIZONTAL);

                if (splitHorizontal) {
                    parentBox.h = box.h / 2;
                    firstChildBox.y = box.y + parentBox.h;
                    firstChildBox.h = box.h - parentBox.h;
                } else {
                    parentBox.w = box.w / 2;
                    firstChildBox.x = box.x + parentBox.w;
                    firstChildBox.w = box.w - parentBox.w;
                }

                auto* firstChild = nodeChildren[0];
                auto& firstChildChildren = childrenMap[firstChild->pTarget.lock()];
                layoutDwindleRecursive(firstChild, firstChildChildren, firstChildBox);

                std::vector<SNstackNodeData*> remainingChildren(nodeChildren.begin() + 1, nodeChildren.end());
                layoutDwindleRecursive(node, remainingChildren, parentBox);
            }
        };

    bool initialSplitHorizontal = (orientation % 2 == 0);
    if (roots.size() == 1) {
        auto& rootChildren = childrenMap[roots[0]->pTarget.lock()];
        layoutDwindleRecursive(roots[0], rootChildren, stackBox);
    } else {
        float rootSize = initialSplitHorizontal ? stackBox.h / roots.size() : stackBox.w / roots.size();
        for (size_t i = 0; i < roots.size(); i++) {
            CBox rootBox = stackBox;
            if (initialSplitHorizontal) {
                rootBox.y = stackBox.y + i * rootSize;
                rootBox.h = rootSize;
            } else {
                rootBox.x = stackBox.x + i * rootSize;
                rootBox.w = rootSize;
            }
            auto& rootChildren = childrenMap[roots[i]->pTarget.lock()];
            layoutDwindleRecursive(roots[i], rootChildren, rootBox);
        }
    }
}

void CNstackAlgorithm::layoutStackVertical(std::vector<SNstackNodeData*>& windows, PHLMONITOR monitor, int orientation, Vector2D stackPos, float stackWidth, float stackNodeSizeLeft) {
    const auto WORKAREA = m_parent.lock()->space()->workArea();

    int nodesInStack = windows.size();
    float nodeCoord = 0;
    float spaceLeft = stackNodeSizeLeft;

    for (size_t i = 0; i < windows.size(); i++) {
        auto* nd = windows[i];

        if (orientation % 2 == 0) {
            nd->position = WORKAREA.pos() + Vector2D(stackPos.x, nodeCoord);
        } else {
            nd->position = WORKAREA.pos() + Vector2D(nodeCoord, stackPos.x);
        }

        float NODESIZE;
        if (i == windows.size() - 1) {
            NODESIZE = spaceLeft;
        } else {
            NODESIZE = (stackNodeSizeLeft / nodesInStack) * nd->percSize;
            if (NODESIZE > spaceLeft * 0.9f)
                NODESIZE = spaceLeft * 0.9f;
        }

        nd->size = orientation % 2 == 1 ? Vector2D(NODESIZE, stackWidth) : Vector2D(stackWidth, NODESIZE);
        spaceLeft -= NODESIZE;
        nodeCoord += NODESIZE;
        applyNodeDataToWindow(nd);
    }
}

void CNstackAlgorithm::recomputeSplitDirections(std::vector<SNstackNodeData*>& windows, int orientation) {
    std::map<SP<Layout::ITarget>, std::vector<SNstackNodeData*>> childrenMap;
    std::vector<SNstackNodeData*> roots;

    for (auto* nd : windows) {
        auto parent = nd->dwindleParent.lock();
        bool parentInStack = false;
        if (parent) {
            for (auto* other : windows) {
                if (other->pTarget.lock() == parent) {
                    parentInStack = true;
                    break;
                }
            }
        }

        if (parentInStack) {
            childrenMap[parent].push_back(nd);
        } else {
            roots.push_back(nd);
        }
    }

    std::function<void(SNstackNodeData*, eSplitDirection)> setSplitDirections =
        [&](SNstackNodeData* node, eSplitDirection direction) {
            node->splitDirection = direction;
            auto& children = childrenMap[node->pTarget.lock()];
            eSplitDirection childDirection = (direction == eSplitDirection::SPLIT_HORIZONTAL)
                ? eSplitDirection::SPLIT_VERTICAL
                : eSplitDirection::SPLIT_HORIZONTAL;
            for (auto* child : children) {
                setSplitDirections(child, childDirection);
            }
        };

    eSplitDirection initialDirection = (orientation % 2 == 0)
        ? eSplitDirection::SPLIT_HORIZONTAL
        : eSplitDirection::SPLIT_VERTICAL;

    for (auto* root : roots) {
        setSplitDirections(root, initialDirection);
    }
}

void CNstackAlgorithm::resizeTarget(const Vector2D& pixResize, SP<Layout::ITarget> target, Layout::eRectCorner corner) {
    const auto PNODE = getNodeFromTarget(target);

    if (!PNODE) {
        // floating - let the window handle it
        auto PWINDOW = target->window();
        if (PWINDOW)
            *PWINDOW->m_realSize = Vector2D(std::max((PWINDOW->m_realSize->goal() + pixResize).x, 20.0), std::max((PWINDOW->m_realSize->goal() + pixResize).y, 20.0));
        return;
    }

    auto space = m_parent.lock()->space();
    const auto PMONITOR = space->workspace()->m_monitor.lock();

    m_forceWarps = true;

    const auto PMASTERNODE = getMasterNode();
    bool       xResizeDone = false;
    bool       yResizeDone = false;

    if (PNODE->isMaster) {
        double delta = 0;

        switch (m_wsData.orientation) {
            case NSTACK_ORIENTATION_LEFT:
                delta       = pixResize.x / PMONITOR->m_size.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_RIGHT:
                delta       = -pixResize.x / PMONITOR->m_size.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_BOTTOM:
                delta       = -pixResize.y / PMONITOR->m_size.y;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_TOP:
                delta       = pixResize.y / PMONITOR->m_size.y;
                yResizeDone = true;
                break;
            case NSTACK_ORIENTATION_HCENTER:
                delta       = pixResize.x / PMONITOR->m_size.x;
                xResizeDone = true;
                break;
            case NSTACK_ORIENTATION_VCENTER:
                delta       = pixResize.y / PMONITOR->m_size.y;
                yResizeDone = true;
                break;
            default: UNREACHABLE();
        }

        for (auto& n : m_nodes) {
            if (n.isMaster) {
                n.percMaster     = std::clamp(n.percMaster + delta, 0.05, 0.95);
                n.masterAdjusted = true;
            }
        }
    }

    if (pixResize.x != 0 && !xResizeDone) {
        if (PNODE->isMaster && getMastersCount() > 1 && m_wsData.orientation % 2 == 1) {
            const auto SIZE = (PMONITOR->m_size.x - PMONITOR->m_reservedArea.left() - PMONITOR->m_reservedArea.right()) / getMastersCount();
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesCount() - getMastersCount()) > 1) {
            if (m_wsData.orientation % 2 == 1) {
                const auto SIZE = (PMONITOR->m_size.x - PMONITOR->m_reservedArea.left() - PMONITOR->m_reservedArea.right()) / m_wsData.stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.x / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    (PMONITOR->m_size.x - PMONITOR->m_reservedArea.left() - PMONITOR->m_reservedArea.right() - PMASTERNODE->size.x) / m_wsData.stackNodeCount.size();
                bool isRightmostStack = false;
                if ((m_wsData.orientation == NSTACK_ORIENTATION_LEFT || m_wsData.orientation == NSTACK_ORIENTATION_RIGHT) &&
                    PNODE->stackNum == (int)m_wsData.stackNodeCount.size() - 1) {
                    isRightmostStack = true;
                }

                if (isRightmostStack && PNODE->stackNum > 1) {
                    m_wsData.stackPercs[PNODE->stackNum - 1] = std::clamp(m_wsData.stackPercs[PNODE->stackNum - 1] + pixResize.x / SIZE, 0.05, 1.95);
                } else {
                    m_wsData.stackPercs[PNODE->stackNum] = std::clamp(m_wsData.stackPercs[PNODE->stackNum] + pixResize.x / SIZE, 0.05, 1.95);
                }
            }
        }
    }

    if (pixResize.y != 0 && !yResizeDone) {
        if (PNODE->isMaster && getMastersCount() > 1 && m_wsData.orientation % 2 == 0) {
            const auto SIZE = (PMONITOR->m_size.y - PMONITOR->m_reservedArea.top() - PMONITOR->m_reservedArea.bottom()) / getMastersCount();
            PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
        } else if (!PNODE->isMaster && (getNodesCount() - getMastersCount()) > 1) {
            if (m_wsData.orientation % 2 == 0) {
                const auto SIZE = (PMONITOR->m_size.y - PMONITOR->m_reservedArea.top() - PMONITOR->m_reservedArea.bottom()) / m_wsData.stackNodeCount[PNODE->stackNum];
                PNODE->percSize = std::clamp(PNODE->percSize + pixResize.y / SIZE, 0.05, 1.95);
            } else {
                const auto SIZE =
                    (PMONITOR->m_size.y - PMONITOR->m_reservedArea.top() - PMONITOR->m_reservedArea.bottom() - PMASTERNODE->size.y) / m_wsData.stackNodeCount.size();
                bool isBottommostStack = false;
                if ((m_wsData.orientation == NSTACK_ORIENTATION_TOP || m_wsData.orientation == NSTACK_ORIENTATION_BOTTOM) &&
                    PNODE->stackNum == (int)m_wsData.stackNodeCount.size() - 1) {
                    isBottommostStack = true;
                }

                if (isBottommostStack && PNODE->stackNum > 1) {
                    m_wsData.stackPercs[PNODE->stackNum - 1] = std::clamp(m_wsData.stackPercs[PNODE->stackNum - 1] + pixResize.y / SIZE, 0.05, 1.95);
                } else {
                    m_wsData.stackPercs[PNODE->stackNum] = std::clamp(m_wsData.stackPercs[PNODE->stackNum] + pixResize.y / SIZE, 0.05, 1.95);
                }
            }
        }
    }

    recalculate();

    m_forceWarps = false;
}

SP<Layout::ITarget> CNstackAlgorithm::getNextTarget(SP<Layout::ITarget> target, bool next) {
    if (!isTargetTiled(target))
        return nullptr;

    const auto PNODE = getNodeFromTarget(target);

    auto       nodes = m_nodes;
    if (!next)
        std::reverse(nodes.begin(), nodes.end());

    const auto NODEIT = std::find(nodes.begin(), nodes.end(), *PNODE);

    const bool ISMASTER = PNODE->isMaster;

    auto CANDIDATE = std::find_if(NODEIT, nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER == other.isMaster; });
    if (CANDIDATE == nodes.end())
        CANDIDATE = std::find_if(nodes.begin(), nodes.end(), [&](const auto& other) { return other != *PNODE && ISMASTER != other.isMaster; });

    return CANDIDATE == nodes.end() ? nullptr : CANDIDATE->pTarget.lock();
}

SP<Layout::ITarget> CNstackAlgorithm::getNextCandidate(SP<Layout::ITarget> old) {
    return getNextTarget(old, true);
}

Config::ErrorResult CNstackAlgorithm::layoutMsg(const std::string_view& sv) {
    auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {};

    auto focusedTarget = PWINDOW->layoutTarget();

    auto switchToWindow = [&](SP<Layout::ITarget> targetToFocus) {
        if (!targetToFocus)
            return;
        auto win = targetToFocus->window();
        if (!validMapped(win))
            return;

        if (PWINDOW->isFullscreen()) {
            const auto FSMODE        = PWINDOW->m_fullscreenState.internal;
            g_pCompositor->setWindowFullscreenInternal(PWINDOW, FSMODE_NONE);
            Desktop::focusState()->fullWindowFocus(win, Desktop::FOCUS_REASON_OTHER);
            if (m_wsData.inherit_fullscreen)
                g_pCompositor->setWindowFullscreenInternal(win, FSMODE);
        } else {
            Desktop::focusState()->fullWindowFocus(win, Desktop::FOCUS_REASON_OTHER);
            g_pCompositor->warpCursorTo(win->middle());
        }
        g_pInputManager->m_forcedFocus = win;
        g_pInputManager->simulateMouseMovement();
        g_pInputManager->m_forcedFocus.reset();
    };

    Hyprutils::String::CVarList2 vars(std::string(sv), 0, ' ');

    if (vars.size() < 1 || vars[0].empty()) {
        Log::logger->log(Log::ERR, "layoutmsg called without params");
        return {};
    }

    auto command = vars[0];

    if (command == "swapwithmaster") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        const auto PMASTER = getMasterNode();
        if (!PMASTER)
            return {};

        auto masterTarget = PMASTER->pTarget.lock();

        if (masterTarget != focusedTarget) {
            const auto NEWMASTER       = focusedTarget;
            const bool newFocusToChild = vars.size() >= 2 && vars[1] == "child";
            swapTargets(NEWMASTER, masterTarget);
            switchToWindow(newFocusToChild ? masterTarget : NEWMASTER);
        } else {
            for (auto& n : m_nodes) {
                if (!n.isMaster) {
                    auto slaveTarget = n.pTarget.lock();
                    swapTargets(slaveTarget, masterTarget);
                    const bool newFocusToMaster = vars.size() >= 2 && vars[1] == "master";
                    switchToWindow(newFocusToMaster ? slaveTarget : masterTarget);
                    break;
                }
            }
        }
        return {};
    } else if (command == "focusmaster") {
        if (!focusedTarget)
            return {};

        const auto PMASTER = getMasterNode();
        if (!PMASTER)
            return {};

        auto masterTarget = PMASTER->pTarget.lock();

        if (masterTarget != focusedTarget) {
            switchToWindow(masterTarget);
        } else if (vars.size() >= 2 && vars[1] == "master") {
            return {};
        } else {
            for (auto& n : m_nodes) {
                if (!n.isMaster) {
                    switchToWindow(n.pTarget.lock());
                    break;
                }
            }
        }
        return {};
    } else if (command == "cyclenext") {
        if (!focusedTarget)
            return {};
        switchToWindow(getNextTarget(focusedTarget, true));
    } else if (command == "cycleprev") {
        if (!focusedTarget)
            return {};
        switchToWindow(getNextTarget(focusedTarget, false));
    } else if (command == "swapnext") {
        if (!focusedTarget)
            return {};
        const auto NEXT = getNextTarget(focusedTarget, true);
        if (NEXT) {
            swapTargets(focusedTarget, NEXT);
            switchToWindow(focusedTarget);
        }
    } else if (command == "swapprev") {
        if (!focusedTarget)
            return {};
        const auto PREV = getNextTarget(focusedTarget, false);
        if (PREV) {
            swapTargets(focusedTarget, PREV);
            switchToWindow(focusedTarget);
        }
    } else if (command == "addmaster") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        const auto PNODE = getNodeFromTarget(focusedTarget);
        if (!PNODE || PNODE->isMaster) {
            for (auto& n : m_nodes) {
                if (!n.isMaster) {
                    n.isMaster = true;
                    break;
                }
            }
        } else {
            PNODE->isMaster = true;
        }
        recalculate();
    } else if (command == "removemaster") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        const auto WINDOWS = getNodesCount();
        const auto MASTERS = getMastersCount();
        if (WINDOWS < 2 || MASTERS < 2)
            return {};

        const auto PNODE = getNodeFromTarget(focusedTarget);
        if (!PNODE || !PNODE->isMaster) {
            for (auto it = m_nodes.rbegin(); it != m_nodes.rend(); it++) {
                if (it->isMaster) {
                    it->isMaster = false;
                    break;
                }
            }
        } else {
            PNODE->isMaster = false;
        }
        recalculate();
    } else if (command == "orientationleft" || command == "orientationright" || command == "orientationtop" || command == "orientationbottom" || command == "orientationcenter" ||
               command == "orientationhcenter" || command == "orientationvcenter") {

        if (command == "orientationleft")
            m_wsData.orientation = NSTACK_ORIENTATION_LEFT;
        else if (command == "orientationright")
            m_wsData.orientation = NSTACK_ORIENTATION_RIGHT;
        else if (command == "orientationtop")
            m_wsData.orientation = NSTACK_ORIENTATION_TOP;
        else if (command == "orientationbottom")
            m_wsData.orientation = NSTACK_ORIENTATION_BOTTOM;
        else if (command == "orientationcenter" || command == "orientationhcenter")
            m_wsData.orientation = NSTACK_ORIENTATION_HCENTER;
        else if (command == "orientationvcenter")
            m_wsData.orientation = NSTACK_ORIENTATION_VCENTER;

        recalculate();
    } else if (command == "orientationnext") {
        runOrientationCycle(nullptr, 1);
    } else if (command == "orientationprev") {
        runOrientationCycle(nullptr, -1);
    } else if (command == "orientationcycle") {
        runOrientationCycle(&vars, 1);
    } else if (command == "resetsplits") {
        resetNodeSplits();
    } else if (command == "setstackcount") {
        if (vars.size() >= 2) {
            int newStackCount = 2;
            switch (vars[1][0]) {
                case '+':
                case '-': newStackCount = m_wsData.m_iStackCount + std::stoi(std::string(vars[1])); break;
                default: newStackCount = std::stoi(std::string(vars[1])); break;
            }
            if (newStackCount) {
                if (newStackCount < 2)
                    newStackCount = 2;
                m_wsData.m_iStackCount = newStackCount;
                recalculate();
            }
        }
    } else if (command == "movetostack") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        int direction = 1;
        if (vars.size() >= 2) {
            if (vars[1] == "l" || vars[1] == "left")
                direction = -1;
        }

        moveToAdjacentStack(focusedTarget, direction);
    } else if (command == "stacklayoutmode") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        const auto PNODE = getNodeFromTarget(focusedTarget);
        if (!PNODE || PNODE->isMaster)
            return {};

        int stackIndex = PNODE->stackNum - 1;
        if (stackIndex < 0 || stackIndex >= (int)m_wsData.stackLayoutModes.size())
            return {};

        std::string mode = vars.size() >= 2 ? std::string(vars[1]) : "toggle";
        bool switchingToDwindle = false;

        if (mode == "toggle") {
            switchingToDwindle = (m_wsData.stackLayoutModes[stackIndex] == eStackLayoutMode::VERTICAL);
            m_wsData.stackLayoutModes[stackIndex] = switchingToDwindle
                ? eStackLayoutMode::DWINDLE : eStackLayoutMode::VERTICAL;
        } else if (mode == "dwindle") {
            switchingToDwindle = true;
            m_wsData.stackLayoutModes[stackIndex] = eStackLayoutMode::DWINDLE;
        } else {
            m_wsData.stackLayoutModes[stackIndex] = eStackLayoutMode::VERTICAL;
        }

        if (switchingToDwindle) {
            std::vector<SNstackNodeData*> stackWindows;
            for (auto& nd : m_nodes) {
                if (!nd.isMaster && nd.stackNum == PNODE->stackNum) {
                    stackWindows.push_back(&nd);
                }
            }
            recomputeSplitDirections(stackWindows, m_wsData.orientation);
        }

        recalculate();
    } else if (command == "togglesplit") {
        if (!focusedTarget || !isTargetTiled(focusedTarget))
            return {};

        const auto PNODE = getNodeFromTarget(focusedTarget);
        if (!PNODE || PNODE->isMaster)
            return {};

        int stackIndex = PNODE->stackNum - 1;
        if (stackIndex < 0 || stackIndex >= (int)m_wsData.stackLayoutModes.size())
            return {};

        if (m_wsData.stackLayoutModes[stackIndex] != eStackLayoutMode::DWINDLE)
            return {};

        PNODE->splitDirection = (PNODE->splitDirection == eSplitDirection::SPLIT_HORIZONTAL)
            ? eSplitDirection::SPLIT_VERTICAL
            : eSplitDirection::SPLIT_HORIZONTAL;

        recalculate();
    }

    return {};
}

void CNstackAlgorithm::runOrientationCycle(Hyprutils::String::CVarList2* vars, int direction) {
    std::vector<eColOrientation> cycle;
    if (vars != nullptr)
        buildOrientationCycleVectorFromVars(cycle, *vars);

    if (cycle.size() == 0)
        buildOrientationCycleVectorFromEOperation(cycle);

    int nextOrPrev = 0;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (m_wsData.orientation == cycle.at(i)) {
            nextOrPrev = i + direction;
            break;
        }
    }

    if (nextOrPrev >= (int)cycle.size())
        nextOrPrev = nextOrPrev % (int)cycle.size();
    else if (nextOrPrev < 0)
        nextOrPrev = cycle.size() + (nextOrPrev % (int)cycle.size());

    m_wsData.orientation = cycle.at(nextOrPrev);
    recalculate();
}

void CNstackAlgorithm::buildOrientationCycleVectorFromEOperation(std::vector<eColOrientation>& cycle) {
    for (int i = 0; i <= NSTACK_ORIENTATION_VCENTER; ++i) {
        cycle.push_back((eColOrientation)i);
    }
}

void CNstackAlgorithm::buildOrientationCycleVectorFromVars(std::vector<eColOrientation>& cycle, Hyprutils::String::CVarList2& vars) {
    for (size_t i = 1; i < vars.size(); ++i) {
        if (vars[i] == "top")
            cycle.push_back(NSTACK_ORIENTATION_TOP);
        else if (vars[i] == "right")
            cycle.push_back(NSTACK_ORIENTATION_RIGHT);
        else if (vars[i] == "bottom")
            cycle.push_back(NSTACK_ORIENTATION_BOTTOM);
        else if (vars[i] == "left")
            cycle.push_back(NSTACK_ORIENTATION_LEFT);
        else if (vars[i] == "hcenter")
            cycle.push_back(NSTACK_ORIENTATION_HCENTER);
        else if (vars[i] == "vcenter")
            cycle.push_back(NSTACK_ORIENTATION_VCENTER);
    }
}

void CNstackAlgorithm::swapTargets(SP<Layout::ITarget> a, SP<Layout::ITarget> b) {
    auto PNODE  = getNodeFromTarget(a);
    auto PNODE2 = getNodeFromTarget(b);

    if (!PNODE2 || !PNODE)
        return;

    // Swap target pointers in nodes
    PNODE->pTarget  = b;
    PNODE2->pTarget = a;

    recalculate();
}

void CNstackAlgorithm::moveTargetInDirection(SP<Layout::ITarget> t, Math::eDirection dir, bool silent) {
    // Find the target in a given direction within this workspace
    auto PWINDOW = t->window();
    if (!PWINDOW)
        return;

    char dirChar;
    switch (dir) {
        case Math::DIRECTION_LEFT:  dirChar = 'l'; break;
        case Math::DIRECTION_RIGHT: dirChar = 'r'; break;
        case Math::DIRECTION_UP:    dirChar = 'u'; break;
        case Math::DIRECTION_DOWN:  dirChar = 'd'; break;
        default: return;
    }

    std::string dirStr(1, dirChar);
    const auto PWINDOW2 = g_pCompositor->getWindowInDirection(PWINDOW, dir);

    if (!PWINDOW2)
        return;

    auto target2 = PWINDOW2->layoutTarget();
    if (!target2)
        return;

    PWINDOW->setAnimationsToMove();

    // Check if target2 is in this workspace (same algorithm instance)
    if (getNodeFromTarget(target2)) {
        // Same workspace — swap
        swapTargets(t, target2);
        if (silent)
            Desktop::focusState()->fullWindowFocus(PWINDOW2, Desktop::FOCUS_REASON_OTHER);
    }
    // Cross-workspace moves are handled by the framework (CSpace/CLayoutManager)
}

void CNstackAlgorithm::moveToAdjacentStack(SP<Layout::ITarget> target, int direction) {
    auto* PNODE = getNodeFromTarget(target);
    if (!PNODE || PNODE->isMaster)
        return;

    int numStacks = m_wsData.stackNodeCount.size() - 1;
    if (numStacks <= 1)
        return;

    int currentStack = PNODE->targetStack >= 0 ? PNODE->targetStack : (PNODE->stackNum - 1);

    int newStack = currentStack + direction;
    if (newStack < 0) {
        newStack = numStacks - 1;
    } else if (newStack >= numStacks) {
        newStack = 0;
    }

    if (newStack == currentStack)
        return;

    PNODE->targetStack = newStack;
    recalculate();
}

std::optional<Vector2D> CNstackAlgorithm::predictSizeForNewTarget() {
    return {};
}
