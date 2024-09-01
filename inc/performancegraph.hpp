/****************************************************************************
**
** Copyright (C) 2023 Philip Seeger (p@c0xc.net)
** This module was developed for CapacityTester.
**
****************************************************************************/

#ifndef PERFORMANCEGRAPH_HPP
#define PERFORMANCEGRAPH_HPP

#include <cassert>

#include <QDebug>
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QTimer>
#include <QCheckBox>
#include <QRadioButton>
#include <QPainter>
#include <QPaintEvent>

class PerformanceGraph : public QWidget
{
    Q_OBJECT

signals:

public:

    struct DataPoint
    {
        qint64 x {};
        double y {};
    };

    PerformanceGraph(const QString &x_label = "", const QString &y_label = "", QWidget *parent = 0);

    void
    setImmediateRepaint();

public slots:

    void
    reset();

    void
    addPoint(const DataPoint &point);

    void
    addPoint(qint64 x, double y);

    void
    addPoint(qint64 x, qint64 y);

private slots:

    void
    paintAxes(QPainter *painter);

    void
    paintPoints(QPainter *painter);

    void
    paintPosCross(QPainter *painter);

    void
    paintEvent(QPaintEvent *event);

    void
    mouseMoveEvent(QMouseEvent *event);

    void
    enterEvent(QEvent *event);

    void
    leaveEvent(QEvent *event);

private:

    void
    scheduleRepaint();

    QRect
    graphRect(QRect *full_rect_ptr = 0);

    DataPoint
    dataCorner();

    QPointF
    dataPointToGraphPoint(const DataPoint &point, QPainter *painter = 0);

    QPointF
    widgetPointToGraphPoint(const QPoint &point);

    DataPoint
    getDataPointForX(qint64 x);

    bool
    m_auto_zoom;

    int
    m_ax_x;

    int
    m_ax_y;

    int
    m_margin;

    bool
    m_immediate_repaint;

    DataPoint
    m_max_pp;

    qint64
    maxX(int min = 0);

    double
    maxY(double min = 0);

    QString
    m_x_label;

    QString
    m_y_label;

    QList<DataPoint>
    m_points;

    QMap<qint64, DataPoint>
    m_x_map;

    bool
    m_pos_cross_on_hover;

    bool
    m_mouse_hover;

    int
    m_pos_cross_at_x;

    double
    m_zoom;



};

#endif
