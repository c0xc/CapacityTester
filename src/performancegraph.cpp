/****************************************************************************
**
** Copyright (C) 2023 Philip Seeger (p@c0xc.net)
** This module was developed for CapacityTester.
**
****************************************************************************/

#include "performancegraph.hpp"

PerformanceGraph::PerformanceGraph(const QString &x_label, const QString &y_label, QWidget *parent)
                : QWidget(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout;
    setLayout(vbox);

    m_x_label = x_label;
    m_y_label = y_label;

    //Set size policy depending on parameters - later...
    //Now, just set a fixed height
    QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    //if (fixed)
    //    policy.setVerticalPolicy(QSizePolicy::Fixed); //forced height
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    setFixedHeight(200);

    m_auto_zoom = true;
    m_ax_x = 0;
    m_ax_y = 0;
    m_immediate_repaint = false;
    m_margin = 15;
    m_pos_cross_at_x = 0;
    m_pos_cross_on_hover = true;
    m_mouse_hover = false;
    m_zoom = 0.5;

    if (m_pos_cross_on_hover)
        setMouseTracking(true);

}

void
PerformanceGraph::setImmediateRepaint()
{
    m_immediate_repaint = true;
}

void
PerformanceGraph::reset()
{
    m_max_pp = DataPoint();
    m_points.clear();
    m_x_map.clear();

    update();
}

void
PerformanceGraph::addPoint(const DataPoint &point)
{
    //Add point, if legal (x > last x)
    if (point.x <= maxX()) return; //TODO raise exception
    m_points << point;
    m_x_map[point.x] = point;

    //Store maximum value
    m_max_pp.x = point.x;
    if (point.y > m_max_pp.y)
        m_max_pp.y = point.y;

    //Schedule repaint (paintEvent()); alternative: repaint() (immediate)
    scheduleRepaint();
}

void
PerformanceGraph::addPoint(qint64 x, double y)
{
    DataPoint point;
    point.x = x;
    point.y = y;
    addPoint(point);
}

void
PerformanceGraph::addPoint(qint64 x, qint64 y)
{
    DataPoint point;
    point.x = x;
    point.y = y;
    addPoint(point);
}

void
PerformanceGraph::paintAxes(QPainter *painter)
{
    //Fill background
    QRect full_rect(this->rect()); //consider *full_rect_ptr
    qint64 width = full_rect.width();
    qint64 height = full_rect.height();
    painter->setBrush(Qt::white);
    painter->drawRect(full_rect);

    //Matrix corner defaults to top-left corner
    int available_width = graphRect().width();
    int available_height = graphRect().height();
    qint64 max_x_val = maxX(1);
    double max_y_val = maxY(1);
    //---
    //X axis
    painter->setBrush(Qt::black);
    painter->setBrush(Qt::gray);
    int margin = m_margin;
    int ax_y = height - margin;
    int ax_x1 = margin;
    int ax_x2 = width - margin;
    painter->drawLine(QPoint(ax_x1, ax_y), QPoint(ax_x2, ax_y));
    QVector<QPointF> arrow_polygon;
    arrow_polygon << QPointF(ax_x2, ax_y);
    arrow_polygon << QPointF(ax_x2 - 3, ax_y - 3);
    arrow_polygon << QPointF(ax_x2 - 3, ax_y + 3);
    painter->drawPolygon(arrow_polygon);
    //---
    //Y axis
    int ax_x = margin;
    int ax_y1 = margin;
    int ax_y2 = height - margin;
    painter->drawLine(QPoint(ax_x, ax_y1), QPoint(ax_x, ax_y2));
    arrow_polygon.clear();
    arrow_polygon << QPointF(ax_x, ax_y1);
    arrow_polygon << QPointF(ax_x - 3, ax_y1 + 3);
    arrow_polygon << QPointF(ax_x + 3, ax_y1 + 3);
    painter->drawPolygon(arrow_polygon);
    //---
    //Y axis label
    QFont font = painter->font();
    font.setPixelSize(10);
    painter->setFont(font);
    painter->rotate(-90);
    QString y_label = QString("%1 %2").arg(max_y_val, 0, 'f', 0).arg(m_y_label);
    painter->drawText(QRect(-available_height, 0, available_height, margin), Qt::AlignRight, y_label);
    painter->rotate(+90);
    //---
    //X axis label
    QString x_label = QString("%1 %2").arg(max_x_val).arg(m_x_label);
    painter->drawText(QRect(0 + margin, ax_y, available_width + margin, margin), Qt::AlignRight, x_label);

    //Change matrix corner to starting tip of x axis, bottom left
    painter->translate(QPoint(ax_x1, ax_y));
    m_ax_x = ax_x1;
    m_ax_y = ax_y;

    //Axis range, label
    //Calculate range in auto-zoom mode
    //---
    //Mark 100% etc., if auto-scaled we show max x/y centered, not top right
    //Otherwise, having max x/y at top right corner,
    //100% y_val == 100% available_height, now 100% y_val == 50% avail...
    int y_mark_top = available_height / 2;
    painter->drawLine(QPoint(0, -y_mark_top), QPoint(-5, -y_mark_top));
    int x_mark_top = available_width / 2;
    painter->drawLine(QPoint(x_mark_top, 0), QPoint(x_mark_top, 5)); //mark/line

    //Axis range labels
    //Initially we've made auto-zoom to show 100% at 50% of the axis
    //but this would make the graph too small. That's why we wrote max val
    //on the marker in the middle of the axis.
    //Instead, we're now preending max val to the unit in the corner,
    //at the end of each axis.
    //There's not enough space for a separate marker, value.
    painter->setPen(Qt::black);
    painter->rotate(-90);
    painter->drawText(QRect(0, -1 -margin, available_height, margin), Qt::AlignHCenter, QString::number(max_y_val / 2, 'f', 1));
    painter->drawText(QRect(0, -1 -margin, available_height, margin), Qt::AlignLeft, QString::number(0));
    painter->rotate(+90);
    painter->drawText(QRect(0, -1, available_width, margin), Qt::AlignHCenter, QString::number(max_x_val / 2));
    painter->drawText(QRect(0, -1, available_width, margin), Qt::AlignLeft, QString::number(0));
    //---

    //Reset matrix corner
    painter->resetTransform();
}

void
PerformanceGraph::paintPoints(QPainter *painter)
{
    //Change matrix corner to starting tip of x axis, bottom left
    painter->translate(QPoint(m_ax_x, m_ax_y));

    QPointF last_point;
    for (qint64 i = 0; i < m_points.size(); i++)
    {
        QPointF point = dataPointToGraphPoint(m_points[i], painter);
        //Draw new point
        painter->setPen(Qt::black);
        painter->drawPoint(point);

        //Connect last point with this point to fill the void inbetween
        //(That void is where we didn't get any data.)
        if (i > 0)
        {
            //Connect the dots
            painter->drawLine(point, last_point);

            //Paint light color between new point and x axis
            painter->setPen(Qt::lightGray);
            painter->setBrush(Qt::lightGray);
            QVector<QPointF> arrow_polygon;
            arrow_polygon << QPointF(point.x(), point.y() + 1);
            arrow_polygon << QPointF(point.x(), 0);
            arrow_polygon << QPointF(last_point.x(), 0);
            arrow_polygon << QPointF(last_point.x(), last_point.y() + 1);
            painter->drawPolygon(arrow_polygon);
            painter->setPen(Qt::black);

        }

        //Store last point
        last_point = point;
    }

    //Reset matrix corner
    painter->resetTransform();
}

void
PerformanceGraph::paintPosCross(QPainter *painter)
{
    QRect graph_rect = graphRect();

    //Cross/lines to be drawn between selected point and axes
    //Highlight nearest data point (mouse is at pos_x)
    int pos_x = m_pos_cross_at_x; //requested x position
    if (pos_x < 0) return;
    DataPoint nearest_point = getDataPointForX(pos_x);
    QPointF graph_point = dataPointToGraphPoint(nearest_point);

    //Fix coordinates for drawing (set origin at x/y 0)
    painter->translate(graph_rect.topLeft());
    //Paint cross blue
    painter->setPen(Qt::blue);

    painter->drawLine(QPoint(0, graph_point.y()), QPoint(graph_rect.width(), graph_point.y()));

    QString text = QString("@ %1 %2: %3 %4").arg(nearest_point.x).arg(m_x_label).arg(nearest_point.y, 0, 'f', 2).arg(m_y_label);
    painter->drawText(graph_point, text);

    painter->setPen(Qt::black);

    //Reset coordinates
    painter->resetTransform();
}

void
PerformanceGraph::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    //In auto-zoom mode, the axes of the graph are not fixed.
    //Every additional point will at least cause the x axis to grow.
    //By default, the axes are drawn so that the current max (value)
    //is at the center of the graph (of its content area).
    //
    //The content area is simply the area above and next to the axes.

    QPainter temp_painter(this);
    QPainter *painter = &temp_painter;

    //Clear graph and paint axis
    //Unless in auto-zoom mode, this step might not be necessary
    //if event->rect() lies within the graph's content area.
    paintAxes(painter);

    //Paint dots
    paintPoints(painter);

    //Paint cross to indicate current position + overlay label (position)
    if (m_pos_cross_at_x)
        paintPosCross(painter);


}

void
PerformanceGraph::mouseMoveEvent(QMouseEvent *event)
{
    QPointF graph_point = widgetPointToGraphPoint(event->pos());
    if (graph_point.x() < 0) return;
    m_pos_cross_at_x = graph_point.x();
    repaint(); //update
}

void
PerformanceGraph::enterEvent(QEvent *event)
{
    Q_UNUSED(event);

    m_mouse_hover = true;
}

void
PerformanceGraph::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);

    m_mouse_hover = false;
    m_pos_cross_at_x = 0;

    scheduleRepaint();
}

void
PerformanceGraph::scheduleRepaint()
{
    //Schedule repaint (paintEvent()); alternative: repaint() (immediate)
    if (m_immediate_repaint)
        repaint();
    else
        update();
    //https://doc.qt.io/qt-5/qwidget.html#update
    //This function does not cause an immediate repaint; instead it schedules a paint event for processing when Qt returns to the main event loop. This permits Qt to optimize for more speed and less flicker than a call to repaint() does.
}

QRect
PerformanceGraph::graphRect(QRect *full_rect_ptr)
{
    //Full rect: size of widget, painter or pixmap that we're drawing on
    //Graph rect: area within full rect, above x axis, right to y axis
    QRect full_rect = rect(); //default to this widget
    if (full_rect_ptr) full_rect = *full_rect_ptr;
    //both painter->window() and painter->viewport() return a larger rect

    //Null position at start of axes
    QRect graph_rect;
    graph_rect.setX(m_ax_x);
    graph_rect.setY(m_ax_y);

    //Available size = size of axes
    int available_width = full_rect.width() - 2* m_margin;
    int available_height = full_rect.height() - 2* m_margin;
    graph_rect.setWidth(available_width);
    graph_rect.setHeight(available_height);
    //NOTE positive height, upwards!
    //Unlike Qt's coordinates, where positive y values are below the x axis.

    return graph_rect;
}

PerformanceGraph::DataPoint
PerformanceGraph::dataCorner()
{
    //Get max value (in auto mode) or max possible value (in fixed mode)
    DataPoint p_max;
    p_max.x = maxX();
    p_max.y = maxY();

    return p_max;
}

QPointF
PerformanceGraph::dataPointToGraphPoint(const DataPoint &point, QPainter *painter) //unused: painter
{
    //Corner p_max defines max data point (i.e., size of graph)
    //In auto-zoom mode, graph is zoomed to fit the full graph,
    //so the current max x value is used as corner.x;
    //corner.x would always show at the right of the graph,
    //above the right end of the x axis.
    DataPoint p_max = dataCorner(); //data points (max)
    int available_width = graphRect().width(); //graph (max)
    int available_height = graphRect().height(); //graph (max)

    //If x == max_x and y == max_y, mtx_point should be av_w/av_h
    //New point.x must always be <= corner.x
    double x_zoom = (double)available_width / p_max.x; //ratio
    double y_zoom = (double)available_height / p_max.y; //ratio
    QPointF mtx_point;
    mtx_point.setX(point.x * x_zoom);
    mtx_point.setY(point.y * y_zoom);
    assert((int)mtx_point.x() <= available_width);
    mtx_point.setY(mtx_point.y() * (-1));
    return mtx_point;
}

QPointF
PerformanceGraph::widgetPointToGraphPoint(const QPoint &point)
{
    //Widget coordinates have their origin at the top-left corner
    //Graph coordinates have their origin where the axes begin, bottom left

    QRect graph_rect = graphRect();
    DataPoint p_max = dataCorner();
    QPointF mtx_point;
    mtx_point.setX(point.x() - graph_rect.x());
    mtx_point.setY(point.y() - graph_rect.y() - graph_rect.height());
    mtx_point.setY(mtx_point.y() * (-1));

    //widget position -> data point position
    double widget_data_ratio_x = (double)graph_rect.width() / p_max.x;
    double widget_data_ratio_y = (double)graph_rect.height() / p_max.y;
    mtx_point.setX(mtx_point.x() / widget_data_ratio_x);
    mtx_point.setY(mtx_point.y() / widget_data_ratio_y);

    return mtx_point;
}

PerformanceGraph::DataPoint
PerformanceGraph::getDataPointForX(qint64 x)
{
    DataPoint point;

    //Get exact y value if data point at x exists
    if (m_x_map.contains(x))
    {
        //Get exact data point (including y) for specified graph x
        point.x = x;
        point.y = m_x_map[x].y;
    }
    else
    {
        //Calculate nearest data point (closest x)
        qint64 prev_x = 0;
        qint64 next_x = 0;
        qint64 x_diff = -1;
        for (int i = 0, ii = m_points.size(); i < ii; i++)
        {
            qint64 cur_x = m_points[i].x;
            x_diff = cur_x - x;
            if (i)
            {
                if (x_diff < 0)
                {
                    //current point is left of point
                    prev_x = cur_x;
                }
                if (x_diff > 0)
                {
                    if (!next_x)
                        next_x = cur_x;
                    else
                        break;
                }
            }
        }
        qint64 nearest_x = 0;
        if (x - prev_x < next_x - x)
            nearest_x = prev_x;
        else
            nearest_x = next_x;

        point.x = nearest_x;
        point.y = m_x_map[nearest_x].y;
    }

    return point;
}

qint64
PerformanceGraph::maxX(int min)
{
    qint64 p = 0;
    if (!m_points.isEmpty())
    {
        p = m_points.constLast().x;
    }
    if (!p && min) p = min;

    return p;
}

double
PerformanceGraph::maxY(double min)
{
    double p = m_max_pp.y;
    if (!p && min) p = min;
    return p;
}

