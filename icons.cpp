#include "icons.h"

#include <QApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QSvgRenderer>
#include <oclero/qlementine/icons/QlementineIcons.hpp>

namespace {

class PaletteIconEngine : public QIconEngine
{
public:
    explicit PaletteIconEngine(const char* path) : m_path(path) {}

    void paint(QPainter* painter, const QRect& rect, QIcon::Mode, QIcon::State) override
    {
        painter->drawPixmap(rect, tinted(rect.size()));
    }

    QPixmap pixmap(const QSize& size, QIcon::Mode, QIcon::State) override
    {
        return tinted(size);
    }

    QIconEngine* clone() const override
    {
        return new PaletteIconEngine(m_path);
    }

private:
    QPixmap tinted(const QSize& size) const
    {
        QSvgRenderer renderer{QString(m_path)};
        QSize target = size.isEmpty() ? renderer.defaultSize() : size;
        QPixmap px(target);
        px.fill(Qt::transparent);
        QPainter p(&px);
        renderer.render(&p);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(px.rect(), qApp->palette().windowText().color());
        return px;
    }

    const char* m_path;
};

} // namespace

void initIcons()
{
    initializeIconTheme();
}

QIcon themedIcon(Icons16 id)
{
    return QIcon(new PaletteIconEngine(iconPath(id)));
}

QIcon qrCodeIcon()
{
    return QIcon(new PaletteIconEngine(":/icons/icons/qrcode-scan.svg"));
}
