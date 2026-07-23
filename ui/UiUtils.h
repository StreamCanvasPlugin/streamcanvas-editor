#pragma once

#include <Qt>
#include <QWidget>
#include <QGridLayout>
#include <QSizePolicy>
#include <QDoubleSpinBox>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QApplication>

// ── UpdateGuard ───────────────────────────────────────────────────────────────
// RAII guard that sets a bool flag on construction and clears it on
// destruction — replaces manual m_updating = true/false pairs, which are
// error-prone when early returns are involved.
struct UpdateGuard {
    explicit UpdateGuard(bool& flag) : m_flag(flag) { m_flag = true; }
    ~UpdateGuard() { m_flag = false; }
    UpdateGuard(const UpdateGuard&) = delete;
    UpdateGuard& operator=(const UpdateGuard&) = delete;
private:
    bool& m_flag;
};

// ── WaitCursor ────────────────────────────────────────────────────────────────
// RAII guard that pushes a Qt::WaitCursor override on construction and pops
// it on destruction — use around blocking file/image I/O so the user sees a
// busy cursor instead of the app appearing to hang.
struct WaitCursor {
    WaitCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursor() { QApplication::restoreOverrideCursor(); }
    WaitCursor(const WaitCursor&) = delete;
    WaitCursor& operator=(const WaitCursor&) = delete;
};

// ── Widget factories ──────────────────────────────────────────────────────────

// Double spinbox with common ribbon defaults.
QDoubleSpinBox* makeSpinBox(double lo, double hi, int decimals,
                             const QString& suffix, int fixedWidth = 0);

// Small muted caption label rendered above controls (10 px, center-aligned).
QLabel* makeTinyLabel(const QString& text);

// Right-aligned 12 px label for ribbon row controls (e.g. "X ", "Width ").
QLabel* makeRibbonLabel(const QString& text);

// Icon-only label
QLabel* makeIconLabel(const QIcon& icon);

// Small checkable or push tool button with an icon.
QToolButton* makeIconToolButton(const QIcon& icon, const QString& tip,
                                 bool checkable = false);

// ── PopupButton ───────────────────────────────────────────────────────────────
// A QToolButton (MenuButtonPopup) whose dropdown opens a QMenu that embeds
// editorWidget via a QWidgetAction.  Click-outside automatically dismisses
// the menu (standard Qt behavior — no custom event filter required).
// The main-area click is also wired to open the menu so the user can click
// either the icon or the arrow.
struct PopupButton {
    QToolButton* button;
    QMenu*       menu;
};

PopupButton makePopupButton(const QString& text, QWidget* editorWidget,
                             QWidget* parent = nullptr);

// ── GridBuilder ───────────────────────────────────────────────────────────────
class GridBuilder {
public:
    explicit GridBuilder(QWidget* parent = nullptr);

    GridBuilder& spacing(int hor, int ver);
    GridBuilder& stretch(int row, int col, int rowStretch = -1, int colStretch = -1);
    GridBuilder& addWidget(QWidget* widget, int row, int col,
                           int rowSpan = 1, int colSpan = 1);
    GridBuilder& align(Qt::Alignment alignment);
    GridBuilder& sizePolicy(QSizePolicy::Policy xSizePolicy,
                             QSizePolicy::Policy ySizePolicy);

    QGridLayout* get() { return m_layout; }

private:
    QGridLayout* m_layout{nullptr};
    QWidget*     m_lastWidget{nullptr};
};
