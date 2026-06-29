#pragma once

#include <QWidget>

class TitleDocument;
class AnimationTimingEditor;
class QDoubleSpinBox;
class QTabBar;
class QGridLayout;
class QPushButton;
class QLabel;
class QSlider;
class QTimer;

enum class AnimationType;
enum class Easing;

class GraphicTimingEditor : public QWidget {
    Q_OBJECT
public:
    explicit GraphicTimingEditor(TitleDocument* doc, QWidget* parent = nullptr);

    void load();
    void clear();
    bool isIn() const;
    bool isDataAnim() const;

signals:
    void animationChanged(int elementIndex, bool isIn, bool isData,
                          AnimationType type, Easing easing, float delay, float duration);
    void scrubTimeChanged(float t);
    void previewStopped();

private slots:
    void onTabChanged(int index);
    void onMaxDurationChanged(double value);
    void onPlay();
    void onStop();
    void onTick();

private:
    enum class PlayState { Stopped, Playing };

    void rebuild();
    float computeAutoMaxDuration() const;
    void setScrubTimeOnAll(float t);

    TitleDocument*  m_doc;
    PlayState       m_playState{PlayState::Stopped};
    float           m_scrubTime{0.0f};

    QTabBar*        m_tabBar;
    QDoubleSpinBox* m_maxDurSpin;
    QPushButton*    m_playBtn;
    QSlider*        m_speedSlider;
    QLabel*         m_timeLabel;
    float           m_playSpeed{1.0f};
    QTimer*         m_playTimer;
    QWidget*        m_content;
    QGridLayout*    m_grid;
    QList<AnimationTimingEditor*> m_editors;
};
