#pragma once

#include <algorithm>
#include <vector>

#include <QString>
#include <QUndoCommand>

#include "engine/element.h"
#include "engine/visual_element.h"
#include "model/TitleDocument.h"
#include "model/UndoCommands.h"

namespace zorderops {

enum class ReorderOp { ToFront, Forward, Backward, ToBack };

inline QString labelFor(ReorderOp op)
{
    switch (op) {
    case ReorderOp::ToFront:  return QStringLiteral("Bring to Front");
    case ReorderOp::Forward:  return QStringLiteral("Bring Forward");
    case ReorderOp::Backward: return QStringLiteral("Send Backward");
    case ReorderOp::ToBack:   return QStringLiteral("Send to Back");
    }
    return {};
}

// Applies a reorder to `target` among its siblings, pushing one undo macro of
// SetElementFieldCmd<int> zOrder updates (or nothing, if the op is a no-op).
inline void applyReorder(TitleDocument* doc, const IElement* target, ReorderOp op)
{
    if (!doc || !target) return;

    IElement* parentEl = target->GetParent();
    if (!parentEl) return;

    // Gather target's siblings the SAME way TitleTreeModel::childrenOf does
    // (keep sibling definition in sync with TitleTreeModel::childrenOf):
    // direct children of the same parent, VisualElements only, sorted
    // descending by zOrder so index 0 = front (topmost).
    std::vector<const IElement*> siblings;
    for (const IElement* child : parentEl->GetChildren()) {
        if (dynamic_cast<const VisualElement*>(child))
            siblings.push_back(child);
    }
    std::stable_sort(siblings.begin(), siblings.end(), [](const IElement* a, const IElement* b) {
        const auto* va = dynamic_cast<const VisualElement*>(a);
        const auto* vb = dynamic_cast<const VisualElement*>(b);
        int za = va ? va->zOrder : 0;
        int zb = vb ? vb->zOrder : 0;
        return za > zb;
    });

    const int n = static_cast<int>(siblings.size());
    int r = -1;
    for (int i = 0; i < n; ++i)
        if (siblings[i] == target) { r = i; break; }
    if (r < 0 || n < 2) return;

    int newRow = r;
    switch (op) {
    case ReorderOp::ToFront:  newRow = 0;                    break;
    case ReorderOp::ToBack:   newRow = n - 1;                break;
    case ReorderOp::Forward:  newRow = std::max(0, r - 1);   break;
    case ReorderOp::Backward: newRow = std::min(n - 1, r + 1); break;
    }
    if (newRow == r) return;

    siblings.erase(siblings.begin() + r);
    siblings.insert(siblings.begin() + newRow, target);

    auto* macro = new QUndoCommand(labelFor(op));
    for (int k = 0; k < n; ++k) {
        const auto* ve = dynamic_cast<const VisualElement*>(siblings[k]);
        if (!ve) continue;
        const int newZ = n - 1 - k;
        if (ve->zOrder != newZ) {
            new SetElementFieldCmd<int>(
                doc, siblings[k]->GetId(), newZ,
                [](VisualElement& e) -> int& { return e.zOrder; },
                "zOrder", -1, macro);
        }
    }

    if (macro->childCount() == 0) { delete macro; return; }
    doc->undoStack()->push(macro);
}

} // namespace zorderops
