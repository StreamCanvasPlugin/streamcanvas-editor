#pragma once

#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <string>

class TitleDocument;

class GraphicProperties : public QWidget {
    Q_OBJECT
public:
    explicit GraphicProperties(TitleDocument* doc, QWidget* parent = nullptr);

    void setSelection(const std::string& graphicId);

private slots:
    void onDocumentChanged();
    void onIdEditingFinished();
    void onZOrderChanged(int value);

private:
    TitleDocument* m_doc;
    std::string m_graphicId;
    bool m_updating{false};

    QLineEdit* m_idEdit;
    QSpinBox* m_zOrderSpin;
};
